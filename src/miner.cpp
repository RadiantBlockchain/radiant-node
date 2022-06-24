// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <miner.h>

#include <amount.h>
#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <config.h>
#include <consensus/activation.h>
#include <consensus/consensus.h>
#include <consensus/merkle.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <hash.h>
#include <net.h>
#include <policy/policy.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <threadsafety.h>
#include <timedata.h>
#include <txmempool.h>
#include <util/moneystr.h>
#include <util/saltedhashers.h>
#include <util/system.h>
#include <validation.h>
#include <validationinterface.h>

#include <algorithm>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>

// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest fee rate of a transaction combined with all
// its ancestors.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

int64_t UpdateTime(CBlockHeader *pblock, const Consensus::Params &params,
                   const CBlockIndex *pindexPrev) {
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime =
        std::max(pindexPrev->GetMedianTimePast() + 1, GetAdjustedTime());

    if (nOldTime < nNewTime) {
        pblock->nTime = nNewTime;
    }

    // Updating time can change work required on testnet:
    if (params.fPowAllowMinDifficultyBlocks) {
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, params);
    }

    return nNewTime - nOldTime;
}

/// Note: This constructor is used in tests. The production code path ends up
/// immediately overwriting these values in DefaultOptions() below.
BlockAssembler::Options::Options()
    : nExcessiveBlockSize(DEFAULT_EXCESSIVE_BLOCK_SIZE),
      nMaxGeneratedBlockSize(DEFAULT_EXCESSIVE_BLOCK_SIZE),
      blockMinFeeRate(DEFAULT_BLOCK_MIN_TX_FEE_PER_KB) {}

BlockAssembler::BlockAssembler(const CChainParams &params,
                               const CTxMemPool &_mempool,
                               const Options &options)
    : chainparams(params), mempool(&_mempool),
      fPrintPriority(gArgs.GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY)) {
    blockMinFeeRate = options.blockMinFeeRate;
    // Limit size to between 1K and options.nExcessiveBlockSize -1K for sanity:
    nMaxGeneratedBlockSize = std::max<uint64_t>(
        1000, std::min<uint64_t>(options.nExcessiveBlockSize - 1000,
                                 options.nMaxGeneratedBlockSize));
    // Calculate the max consensus sigchecks for this block.
    auto nMaxBlockSigChecks =
        GetMaxBlockSigChecksCount(options.nExcessiveBlockSize);
    // Allow the full amount of signature check operations in lieu of a separate
    // config option. (We are mining relayed transactions with validity cached
    // by everyone else, and so the block will propagate quickly, regardless of
    // how many sigchecks it contains.)
    nMaxGeneratedBlockSigChecks = nMaxBlockSigChecks;
}

static BlockAssembler::Options DefaultOptions(const Config &config) {
    // Block resource limits
    BlockAssembler::Options options;

    options.nExcessiveBlockSize = config.GetExcessiveBlockSize();
    options.nMaxGeneratedBlockSize = config.GetGeneratedBlockSize();

    if (Amount n = Amount::zero();
            gArgs.IsArgSet("-blockmintxfee") && ParseMoney(gArgs.GetArg("-blockmintxfee", ""), n)) {
        options.blockMinFeeRate = CFeeRate(n);
    }

    return options;
}

BlockAssembler::BlockAssembler(const Config &config, const CTxMemPool &_mempool)
    : BlockAssembler(config.GetChainParams(), _mempool,
                     DefaultOptions(config)) {}

void BlockAssembler::resetBlock() {
    // Reserve space for coinbase tx.
    nBlockSize = 1000;
    nBlockSigChecks = 100;

    // These counters do not include coinbase tx.
    nBlockTx = 0;
    nFees = Amount::zero();
}

std::unique_ptr<CBlockTemplate>
BlockAssembler::CreateNewBlock(const CScript &scriptPubKeyIn, double timeLimitSecs, bool checkValidity) {
    const int64_t nTimeStart = GetTimeMicros();

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    // Pointer for convenience.
    pblock = &pblocktemplate->block;

    // Add dummy coinbase tx as first transaction.  It is updated at the end.
    pblocktemplate->entries.emplace_back(CTransactionRef(), -SATOSHI, -1);

    LOCK2(cs_main, mempool->cs);
    CBlockIndex *pindexPrev = ::ChainActive().Tip();
    assert(pindexPrev != nullptr);
    nHeight = pindexPrev->nHeight + 1;

    const Consensus::Params &consensusParams = chainparams.GetConsensus();

    pblock->nVersion = ComputeBlockVersion(pindexPrev, consensusParams);
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand()) {
        pblock->nVersion = gArgs.GetArg("-blockversion", pblock->nVersion);
    }

    pblock->nTime = GetAdjustedTime();
    nMedianTimePast = pindexPrev->GetMedianTimePast();
    nLockTimeCutoff =
        (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
            ? nMedianTimePast
            : pblock->GetBlockTime();

    // CreateNewBlock's CPU time is divided between two parts: addTxs(), and TestBlockValidity(). Our goal is to
    // finish TestBlockValidity by timeLimitSecs, but to do that we have to know when to stop adding transactions in
    // addTxs(). addTxsFrac stores an exponential moving average of what fraction of previous CreateNewBlock
    // run times addTxs() was responsible for. This fraction can change depending on the transaction mix (e.g.
    // tx size, tx chain length).
 
    static double addTxsFrac GUARDED_BY(cs_main) = 0.5; // initial value: 50%
    // Clamp to sane range: [10% - 100%]
    addTxsFrac = std::clamp(addTxsFrac, 0.1, 1.);
    int64_t nAddTxsTimeLimit = 0; // a time point in the future to stop adding; 0 indicates no limit
    if (timeLimitSecs > 0.) {
        // If we are using the time limit feature, then estimate the amount of time we need to spend in addTxs()
        // based on the addTxsFrac statistic, and convert that time into a timepoint (in micros) after nTimeStart.
        timeLimitSecs = std::min(timeLimitSecs, 1e3); // limit to 1e3 secs, to prevent int64 overflow below
        nAddTxsTimeLimit = nTimeStart + static_cast<int64_t>(addTxsFrac * timeLimitSecs * 1e6);
    }

    addTxs(nAddTxsTimeLimit);

    const int64_t nTime0 = GetTimeMicros();

    // Copy all the transactions refs into the block
    pblock->vtx.reserve(pblocktemplate->entries.size());
    for (const CBlockTemplateEntry &entry : pblocktemplate->entries) {
        pblock->vtx.push_back(entry.tx);
    }

    const int64_t nTime1 = GetTimeMicros();

    nLastBlockTx = nBlockTx;
    nLastBlockSize = nBlockSize;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout = COutPoint();
    coinbaseTx.vout.resize(1);
    coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;
    coinbaseTx.vout[0].nValue = nFees + GetBlockSubsidy(nHeight, consensusParams);
    coinbaseTx.vin[0].scriptSig = CScript() << ScriptInt::fromIntUnchecked(nHeight); // << OP_0;

    // Make sure the coinbase is big enough.
    uint64_t coinbaseSize = ::GetSerializeSize(coinbaseTx, PROTOCOL_VERSION);
    if (coinbaseSize < MIN_TX_SIZE) {
        coinbaseTx.vin[0].scriptSig << std::vector<uint8_t>(MIN_TX_SIZE - coinbaseSize - 1);
    }

    pblocktemplate->entries[0].tx = MakeTransactionRef(coinbaseTx);
    pblocktemplate->entries[0].fees = -1 * nFees;
    pblock->vtx[0] = pblocktemplate->entries[0].tx;

    const uint64_t nByteSize =
            checkValidity ? GetSerializeSize(*pblock, PROTOCOL_VERSION)
                          : nBlockSize; // if not checking validity, skip re-serializing the block and estimate the size

    LogPrintf("CreateNewBlock(): %s: %u txs: %u fees: %ld sigchecks %d\n",
              checkValidity ? "total size" : "estimated size", nByteSize, nBlockTx, nFees, nBlockSigChecks);

    // Fill in header.
    pblock->hashPrevBlock = pindexPrev->GetBlockHash();
    UpdateTime(pblock, consensusParams, pindexPrev);
    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);
    pblock->nNonce = 0;
    pblocktemplate->entries[0].sigChecks = 0;

    if (checkValidity) {
        CValidationState state;
        if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev,
                               BlockValidationOptions(GetConfig())
                                   .withCheckPoW(false)
                                   .withCheckMerkleRoot(false))) {
            throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s",
                                               __func__,
                                               FormatStateMessage(state)));
        }
    }

    const int64_t nTime2 = GetTimeMicros();

    // Save time taken by addTxs() vs total time taken
    const int64_t elapsedAddTxs = nTime0 - nTimeStart;
    const int64_t elapsedTotal = nTime2 - nTimeStart;
    // Adjust addTxsFrac based on elapsedAddTxs this run, using an EMA with alpha = 25% for non-tiny blocks
    const double alpha = pblock->vtx.size() > 50 ? 0.25 : 0.05;
    const double thisAddTxsFrac = elapsedTotal > 0 ? std::clamp(elapsedAddTxs / double(elapsedTotal), 0., 1.) : 0.;
    addTxsFrac = addTxsFrac * (1. - alpha) + thisAddTxsFrac * alpha;

    LogPrint(BCLog::BENCH,
             "CreateNewBlock() addTxs: %.2fms, "
             "CTOR: %.2fms, validity: %.2fms (total %.2fms), addTxsFrac: %1.2f, timeLimitSecs: %1.3f\n",
             0.001 * elapsedAddTxs,
             0.001 * (nTime1 - nTime0), 0.001 * (nTime2 - nTime1),
             0.001 * elapsedTotal, addTxsFrac, timeLimitSecs);

    return std::move(pblocktemplate);
}

bool BlockAssembler::TestTx(uint64_t txSize, int64_t txSigChecks) const {
    if (nBlockSize + txSize >= nMaxGeneratedBlockSize) {
        return false;
    }

    if (nBlockSigChecks + txSigChecks >= nMaxGeneratedBlockSigChecks) {
        return false;
    }

    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter) {
    pblocktemplate->entries.emplace_back(iter->GetSharedTx(), iter->GetFee(),
                                         iter->GetSigChecks());
    nBlockSize += iter->GetTxSize();
    ++nBlockTx;
    nBlockSigChecks += iter->GetSigChecks();
    nFees += iter->GetFee();

    if (fPrintPriority) {
        LogPrintf(
            "fee %s txid %s\n",
            CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
            iter->GetTx().GetId().ToString());
    }
}

bool BlockAssembler::CheckTx(const CTransaction &tx) const {
    CValidationState state;
    return ContextualCheckTransaction(chainparams.GetConsensus(),
                                      tx, state, nHeight, nLockTimeCutoff, nMedianTimePast);
}

/**
 * addTxs includes transactions paying a fee by ensuring that
 * the partial ordering of transactions is maintained.  That is to say
 * children come after parents, despite having a potentially larger fee.
 * @param nLimitTimePoint  A time point in the future (obtained via
 *                         GetTimeMicros() + delta). If this argument is > 0,
 *                         then stop looping after this time point elapses.
 *                         Otherwise if <= 0, loop until the block template
 *                         is filled to -blockmaxsize capacity (or until all
 *                         tx's in mempool are added, whichever is smaller).
 */
void BlockAssembler::addTxs(int64_t nLimitTimePoint) {
    using EntryPtrHasher = StdHashWrapper<const CTxMemPoolEntry *>;
    using ParentCountMap = std::unordered_map<const CTxMemPoolEntry *, size_t, EntryPtrHasher>;
    using ChildSet = std::unordered_set<const CTxMemPoolEntry *, EntryPtrHasher>;

    // mapped_value is the number of mempool parents that are still needed for the entry.
    // We decrement this count each time we add a parent of the entry to the block.
    ParentCountMap missingParentCount;
    // set of children we skipped because we have not yet added their parents
    ChildSet skippedChildren;
    missingParentCount.reserve(mempool->size() / 2);
    skippedChildren.reserve(mempool->size() / 2);

    auto TrackSkippedChild = [&skippedChildren](const auto &it) { skippedChildren.insert(&*it); };
    auto IsSkippedChild = [&skippedChildren](const auto &it) { return bool(skippedChildren.count(&*it)); };

    auto MissingParents = [this, &missingParentCount](const auto &iter) EXCLUSIVE_LOCKS_REQUIRED(mempool->cs) {
        // If we've added any of this tx's parents already, then missingParentCount will have the current count
        if (auto pcIt = missingParentCount.find(&*iter); pcIt != missingParentCount.end())
            return pcIt->second != 0; // when pcIt->second reaches 0, we have added all of this tx's parents
        return !mempool->GetMemPoolParents(iter).empty();
    };

    auto TrackParentAdded = [this, &missingParentCount](const auto & child) EXCLUSIVE_LOCKS_REQUIRED(mempool->cs) {
        const auto& [parentCount, inserted] = missingParentCount.try_emplace(&*child, 0 /* dummy */);
        if (inserted) {
            // We haven't processed any of this child tx's parents before,
            // so we're adding the first of its in-mempool parents
            parentCount->second = mempool->GetMemPoolParents(child).size();
        }
        assert(parentCount->second > 0);
        return --parentCount->second == 0;
    };

    auto TimedOut = [nLimitTimePoint] {
        return nLimitTimePoint > 0 && GetTimeMicros() >= nLimitTimePoint;
    };

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    // Transactions that may or may not have been skipped due to parent not
    // being present in the block, but where a parent has now been added.
    std::queue<CTxMemPool::txiter> backlog;

    CTxMemPool::txiter iter;
    auto mi = mempool->mapTx.get<modified_feerate>().begin();
    while (!TimedOut() && (!backlog.empty() || mi != mempool->mapTx.get<modified_feerate>().end())) {

        // Get a new or old transaction in mapTx to evaluate.
        bool isFromBacklog = false;
        if (!backlog.empty()) {
            iter = backlog.front();
            backlog.pop();
            isFromBacklog = true;
        } else {
            iter = mempool->mapTx.project<0>(mi++);
        }

        if (iter->GetModifiedFeeRate() < blockMinFeeRate) {
            break;
        }

        // Check whether all of this tx's parents are already in the block. If
        // not, pass on it until later.
        //
        // If it's from the backlog, then we know all parents are already in
        // the block.
        if (!isFromBacklog && MissingParents(iter)) {
            TrackSkippedChild(iter);
            continue;
        }

        // Check whether the tx will exceed the block limits.
        if (!TestTx(iter->GetTxSize(), iter->GetSigChecks())) {
            ++nConsecutiveFailed;
            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockSize > nMaxGeneratedBlockSize - 1000) {
                // Give up if we're close to full and haven't succeeded in a while.
                break;
            }
            continue;
        }

        // Test transaction finality (locktime)
        if (!CheckTx(iter->GetTx())) {
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Tx can be added.
        AddToBlock(iter);

        // This tx's children may now be candidates for addition if they have
        // higher scores than the tx at the cursor. We can only process a
        // child once all of that tx's parents have been added, though. To
        // avoid O(n^2) checking of dependencies, we store and decrement the
        // number of mempool parents for each child. Although this code
        // ends up taking O(n) time to process a single tx with n children,
        // that's okay because the amount of time taken is proportional to the
        // tx's byte size and fee paid.
        for (const auto& child : mempool->GetMemPoolChildren(iter)) {
            const bool allParentsAdded = TrackParentAdded(child);
            // If all parents have been added to the block, and if this child
            // has been previously skipped due to missing parents, enqueue it
            // (if it hasn't been skipped it will come up in a later iteration)
            if (allParentsAdded && IsSkippedChild(child)) {
                backlog.push(child);
            }
        }
    }
}

static
std::vector<uint8_t> getExcessiveBlockSizeSig(uint64_t nExcessiveBlockSize) {
    std::string cbmsg = "/EB" + getSubVersionEB(nExcessiveBlockSize) + "/";
    return std::vector<uint8_t>(cbmsg.begin(), cbmsg.end());
}

void IncrementExtraNonce(CBlock *pblock, const CBlockIndex *pindexPrev,
                         uint64_t nExcessiveBlockSize,
                         unsigned int &nExtraNonce) {
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock) {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }

    ++nExtraNonce;
    // Height first in coinbase required for block.version=2
    unsigned int nHeight = pindexPrev->nHeight + 1;
    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig =
        (CScript() << ScriptInt::fromIntUnchecked(nHeight)
                   << CScriptNum::fromIntUnchecked(nExtraNonce)
                   << getExcessiveBlockSizeSig(nExcessiveBlockSize)) +
        COINBASE_FLAGS;

    // Make sure the coinbase is big enough.
    uint64_t coinbaseSize = ::GetSerializeSize(txCoinbase, PROTOCOL_VERSION);
    if (coinbaseSize < MIN_TX_SIZE) {
        txCoinbase.vin[0].scriptSig
            << std::vector<uint8_t>(MIN_TX_SIZE - coinbaseSize - 1);
    }

    assert(txCoinbase.vin[0].scriptSig.size() <= MAX_COINBASE_SCRIPTSIG_SIZE);
    assert(::GetSerializeSize(txCoinbase, PROTOCOL_VERSION) >= MIN_TX_SIZE);

    pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}
