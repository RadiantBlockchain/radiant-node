// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
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
#include <util/system.h>
#include <validation.h>
#include <validationinterface.h>

#include <algorithm>
#include <queue>
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

uint64_t CTxMemPoolModifiedEntry::GetVirtualSizeWithAncestors() const {
    return GetVirtualTransactionSize(nSizeWithAncestors,
                                     nSigOpCountWithAncestors);
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
    : chainparams(params), mempool(&_mempool) {
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
    inBlock.clear();

    // Reserve space for coinbase tx.
    nBlockSize = 1000;
    nBlockSigOps = 100;

    // These counters do not include coinbase tx.
    nBlockTx = 0;
    nFees = Amount::zero();
}

std::unique_ptr<CBlockTemplate>
BlockAssembler::CreateNewBlock(const CScript &scriptPubKeyIn, double timeLimitSecs) {
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

    // CreateNewBlock's CPU time is divided between two parts: addPackageTxs(), and TestBlockValidity(). Our goal is to
    // finish TestBlockValidity by timeLimitSecs, but to do that we have to know when to stop adding transactions in
    // addPackageTxs(). addPackageFrac stores an exponential moving average of what fraction of previous CreateNewBlock
    // run times addPackageTxs() was responsible for. This fraction can change depending on the transaction mix (e.g.
    // tx size, tx chain length).
    static double addPackageFrac GUARDED_BY(cs_main) = 0.5; // initial value: 50%
    // Clamp to sane range: [10% - 100%]
    addPackageFrac = std::clamp(addPackageFrac, 0.1, 1.);
    int64_t nAddPackageLimitTime = 0; // a time point in the future to stop adding; 0 indicates no limit
    if (timeLimitSecs > 0.) {
        // If we are using the time limit feature, then estimate the amount of time we need to spend in addPackageTxs()
        // based on the addPackageFrac statistic, and convert that time into a timepoint (in micros) after nTimeStart.
        timeLimitSecs = std::min(timeLimitSecs, 1e3); // limit to 1e3 secs, to prevent int64 overflow below
        nAddPackageLimitTime = nTimeStart + static_cast<int64_t>(addPackageFrac * timeLimitSecs * 1e6);
    }

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    addPackageTxs(nPackagesSelected, nDescendantsUpdated, nAddPackageLimitTime);

    const int64_t nTime0 = GetTimeMicros();

    if (IsMagneticAnomalyEnabled(consensusParams, pindexPrev)) {
        // If magnetic anomaly is enabled, we make sure transaction are
        // canonically ordered.
        std::sort(std::begin(pblocktemplate->entries) + 1,
                  std::end(pblocktemplate->entries),
                  [](const CBlockTemplateEntry &a, const CBlockTemplateEntry &b)
                      -> bool { return a.tx->GetId() < b.tx->GetId(); });
    }

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
    coinbaseTx.vout[0].nValue =
        nFees + GetBlockSubsidy(nHeight, consensusParams);
    coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0;

    // Make sure the coinbase is big enough.
    uint64_t coinbaseSize = ::GetSerializeSize(coinbaseTx, PROTOCOL_VERSION);
    if (coinbaseSize < MIN_TX_SIZE) {
        coinbaseTx.vin[0].scriptSig
            << std::vector<uint8_t>(MIN_TX_SIZE - coinbaseSize - 1);
    }

    pblocktemplate->entries[0].tx = MakeTransactionRef(coinbaseTx);
    pblocktemplate->entries[0].fees = -1 * nFees;
    pblock->vtx[0] = pblocktemplate->entries[0].tx;

    uint64_t nSerializeSize = GetSerializeSize(*pblock, PROTOCOL_VERSION);

    LogPrintf("CreateNewBlock(): total size: %u txs: %u fees: %ld sigops %d\n",
              nSerializeSize, nBlockTx, nFees, nBlockSigOps);

    // Fill in header.
    pblock->hashPrevBlock = pindexPrev->GetBlockHash();
    UpdateTime(pblock, consensusParams, pindexPrev);
    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);
    pblock->nNonce = 0;
    pblocktemplate->entries[0].sigOpCount = 0;

    CValidationState state;
    if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev,
                           BlockValidationOptions(GetConfig())
                               .withCheckPoW(false)
                               .withCheckMerkleRoot(false))) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s",
                                           __func__,
                                           FormatStateMessage(state)));
    }
    const int64_t nTime2 = GetTimeMicros();

    // Save time taken by addPackageTxs() vs total time taken
    const int64_t elapsedAddPkgTxs = nTime0 - nTimeStart;
    const int64_t elapsedTotal = nTime2 - nTimeStart;
    // Adjust addPackageFrac based on elapsedAddPkgTxs this run, using an EMA with alpha = 25% for non-tiny blocks
    const double alpha = pblock->vtx.size() > 50 ? 0.25 : 0.05;
    const double thisAddPackageFrac = elapsedTotal > 0 ? std::clamp(elapsedAddPkgTxs / double(elapsedTotal), 0., 1.) : 0.;
    addPackageFrac = addPackageFrac * (1. - alpha) + thisAddPackageFrac * alpha;

    LogPrint(BCLog::BENCH,
             "CreateNewBlock() packages: %.2fms (%d packages, %d updated descendants), "
             "CTOR: %.2fms, validity: %.2fms (total %.2fms), addPackageFrac: %1.2f, timeLimitSecs: %1.3f\n",
             0.001 * elapsedAddPkgTxs, nPackagesSelected,
             nDescendantsUpdated, 0.001 * (nTime1 - nTime0), 0.001 * (nTime2 - nTime1),
             0.001 * elapsedTotal, addPackageFrac, timeLimitSecs);

    return std::move(pblocktemplate);
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries &testSet) {
    for (CTxMemPool::setEntries::iterator iit = testSet.begin();
         iit != testSet.end();) {
        // Only test txs not already in the block.
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        } else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize,
                                 int64_t packageSigOps) const {
    auto blockSizeWithPackage = nBlockSize + packageSize;
    if (blockSizeWithPackage >= nMaxGeneratedBlockSize) {
        return false;
    }

    if (nBlockSigOps + packageSigOps >= nMaxGeneratedBlockSigChecks) {
        return false;
    }

    return true;
}

/**
 * Perform transaction-level checks before adding to block:
 * - Transaction finality (locktime)
 * - Serialized size (in case -blockmaxsize is in use)
 */
bool BlockAssembler::TestPackageTransactions(
    const CTxMemPool::setEntries &package) {
    uint64_t nPotentialBlockSize = nBlockSize;
    for (CTxMemPool::txiter it : package) {
        CValidationState state;
        if (!ContextualCheckTransaction(chainparams.GetConsensus(), it->GetTx(),
                                        state, nHeight, nLockTimeCutoff,
                                        nMedianTimePast)) {
            return false;
        }

        uint64_t nTxSize = ::GetSerializeSize(it->GetTx(), PROTOCOL_VERSION);
        if (nPotentialBlockSize + nTxSize >= nMaxGeneratedBlockSize) {
            return false;
        }

        nPotentialBlockSize += nTxSize;
    }

    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter) {
    pblocktemplate->entries.emplace_back(iter->GetSharedTx(), iter->GetFee(),
                                         iter->GetSigOpCount());
    nBlockSize += iter->GetTxSize();
    ++nBlockTx;
    nBlockSigOps += iter->GetSigOpCount();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    bool fPrintPriority =
        gArgs.GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        LogPrintf(
            "fee %s txid %s\n",
            CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
            iter->GetTx().GetId().ToString());
    }
}

int BlockAssembler::UpdatePackagesForAdded(
    const CTxMemPool::setEntries &alreadyAdded,
    indexed_modified_transaction_set &mapModifiedTx) {
    int nDescendantsUpdated = 0;
    for (CTxMemPool::txiter it : alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool->CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set.
        for (CTxMemPool::txiter desc : descendants) {
            if (alreadyAdded.count(desc)) {
                continue;
            }

            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                modEntry.nSizeWithAncestors -= it->GetTxSize();
                modEntry.nModFeesWithAncestors -= it->GetModifiedFee();
                modEntry.nSigOpCountWithAncestors -= it->GetSigOpCount();
                mapModifiedTx.insert(modEntry);
            } else {
                mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
            }
        }
    }

    return nDescendantsUpdated;
}

// Skip entries in mapTx that are already in a block or are present in
// mapModifiedTx (which implies that the mapTx ancestor state is stale due to
// ancestor inclusion in the block). Also skip transactions that we've already
// failed to add. This can happen if we consider a transaction in mapModifiedTx
// and it fails: we can then potentially consider it again while walking mapTx.
// It's currently guaranteed to fail again, but as a belt-and-suspenders check
// we put it in failedTx and avoid re-evaluation, since the re-evaluation would
// be using cached size/sigops/fee values that are not actually correct.
bool BlockAssembler::SkipMapTxEntry(
    CTxMemPool::txiter it, indexed_modified_transaction_set &mapModifiedTx,
    CTxMemPool::setEntries &failedTx) {
    assert(it != mempool->mapTx.end());
    return mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it);
}

void BlockAssembler::SortForBlock(
    const CTxMemPool::setEntries &package,
    std::vector<CTxMemPool::txiter> &sortedEntries) {
    // Sort package by ancestor count. If a transaction A depends on transaction
    // B, then A's ancestor count must be greater than B's. So this is
    // sufficient to validly order the transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(),
              CompareTxIterByAncestorCount());
}

/**
 * addPackageTx includes transactions paying a fee by ensuring that
 * the partial ordering of transactions is maintained.  That is to say
 * children come after parents, despite having a potentially larger fee.
 * @param[out] nPackagesSelected    How many packages were selected
 * @param[out] nDescendantsUpdated  Number of descendant transactions updated
 * @param nLimitTimePoint  A time point in the future (obtained via
 *                         GetTimeMicros() + delta). If this argument is > 0,
 *                         then stop looping after this time point elapses.
 *                         Otherwise if <= 0, loop until the block template
 *                         is filled to -blockmaxsize capacity (or until all
 *                         tx's in mempool are added, whichever is smaller).
 */
void BlockAssembler::addPackageTxs(int &nPackagesSelected,
                                   int &nDescendantsUpdated,
                                   int64_t nLimitTimePoint) {
    // selection algorithm orders the mempool based on feerate of a
    // transaction including all unconfirmed ancestors. Since we don't remove
    // transactions from the mempool as we select them for block inclusion, we
    // need an alternate method of updating the feerate of a transaction with
    // its not-yet-selected ancestors as we go. This is accomplished by
    // walking the in-mempool descendants of selected transactions and storing
    // a temporary modified state in mapModifiedTxs. Each time through the
    // loop, we compare the best transaction in mapModifiedTxs with the next
    // transaction in the mempool to decide what transaction package to work
    // on next.

    // mapModifiedTx will store sorted packages after they are modified because
    // some of their txs are already in the block.
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work.
    CTxMemPool::setEntries failedTx;

    // Start by adding all descendants of previously added txs to mapModifiedTx
    // and modifying them for their already included ancestors.
    UpdatePackagesForAdded(inBlock, mapModifiedTx);

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator
        mi = mempool->mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while ((nLimitTimePoint <= 0 || GetTimeMicros() < nLimitTimePoint) &&
           (mi != mempool->mapTx.get<ancestor_score>().end() ||
            !mapModifiedTx.empty())) {
        // First try to find a new transaction in mapTx to evaluate.
        if (mi != mempool->mapTx.get<ancestor_score>().end() &&
            SkipMapTxEntry(mempool->mapTx.project<0>(mi), mapModifiedTx,
                           failedTx)) {
            ++mi;
            continue;
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool->mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry.
            iter = mempool->mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                CompareTxMemPoolEntryByAncestorFee()(
                    *modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score than the one
                // from mapTx. Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        Amount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOps = iter->GetSigOpCountWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOps = modit->nSigOpCountWithAncestors;
        }

        if (packageFees < blockMinFeeRate.GetFee(packageSize)) {
            // Don't include this package, but don't stop yet because something
            // else we might consider may have a sufficient fee rate (since txes
            // are ordered by virtualsize feerate, not actual feerate).
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx, we
                // must erase failed entries so that we can consider the next
                // best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // The following must not use virtual size since TestPackage relies on
        // having an accurate call to
        // GetMaxBlockSigOpsCount(blockSizeWithPackage).
        if (!TestPackage(packageSize, packageSigOps)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx, we
                // must erase failed entries so that we can consider the next
                // best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES &&
                nBlockSize > nMaxGeneratedBlockSize - 1000) {
                // Give up if we're close to full and haven't succeeded in a
                // while.
                break;
            }

            continue;
        }

        CTxMemPool::setEntries ancestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        mempool->CalculateMemPoolAncestors(*iter, ancestors, nNoLimit, nNoLimit,
                                           nNoLimit, nNoLimit, dummy, false);

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final.
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, sortedEntries);

        for (auto &entry : sortedEntries) {
            AddToBlock(entry);
            // Erase from the modified set, if present
            mapModifiedTx.erase(entry);
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(ancestors, mapModifiedTx);
    }
}

static const std::vector<uint8_t>
getExcessiveBlockSizeSig(uint64_t nExcessiveBlockSize) {
    std::string cbmsg = "/EB" + getSubVersionEB(nExcessiveBlockSize) + "/";
    std::vector<uint8_t> vec(cbmsg.begin(), cbmsg.end());
    return vec;
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
        (CScript() << nHeight << CScriptNum(nExtraNonce)
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
