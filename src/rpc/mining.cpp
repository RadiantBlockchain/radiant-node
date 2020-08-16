// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <blockvalidity.h>
#include <chain.h>
#include <chainparams.h>
#include <config.h>
#include <consensus/activation.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <fs.h>
#include <gbtlight.h>
#include <key_io.h>
#include <miner.h>
#include <net.h>
#include <policy/policy.h>
#include <pow.h>
#include <rpc/blockchain.h>
#include <rpc/mining.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <shutdown.h>
#include <txmempool.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <validation.h>
#include <validationinterface.h>
#include <warnings.h>

#include <univalue.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <list>
#include <memory>

/**
 * Return average network hashes per second based on the last 'lookup' blocks,
 * or from the last difficulty change if 'lookup' is nonpositive. If 'height' is
 * nonnegative, compute the estimate at the time when a given block was found.
 */
static UniValue GetNetworkHashPS(int lookup, int height) {
    CBlockIndex *pb = ::ChainActive().Tip();

    if (height >= 0 && height < ::ChainActive().Height()) {
        pb = ::ChainActive()[height];
    }

    if (pb == nullptr || !pb->nHeight) {
        return 0;
    }

    // If lookup is -1, then use blocks since last difficulty change.
    if (lookup <= 0) {
        lookup = pb->nHeight %
                     Params().GetConsensus().DifficultyAdjustmentInterval() +
                 1;
    }

    // If lookup is larger than chain, then set it to chain length.
    if (lookup > pb->nHeight) {
        lookup = pb->nHeight;
    }

    CBlockIndex *pb0 = pb;
    int64_t minTime = pb0->GetBlockTime();
    int64_t maxTime = minTime;
    for (int i = 0; i < lookup; i++) {
        pb0 = pb0->pprev;
        int64_t time = pb0->GetBlockTime();
        minTime = std::min(time, minTime);
        maxTime = std::max(time, maxTime);
    }

    // In case there's a situation where minTime == maxTime, we don't want a
    // divide by zero exception.
    if (minTime == maxTime) {
        return 0;
    }

    arith_uint256 workDiff = pb->nChainWork - pb0->nChainWork;
    int64_t timeDiff = maxTime - minTime;

    return workDiff.getdouble() / timeDiff;
}

static UniValue getnetworkhashps(const Config &config,
                                 const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            "getnetworkhashps ( nblocks height )\n"
            "\nReturns the estimated network hashes per second based on the "
            "last n blocks.\n"
            "Pass in [blocks] to override # of blocks, -1 specifies since last "
            "difficulty change.\n"
            "Pass in [height] to estimate the network speed at the time when a "
            "certain block was found.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric, optional, default=120) The number of "
            "blocks, or -1 for blocks since last difficulty change.\n"
            "2. height      (numeric, optional, default=-1) To estimate at the "
            "time of the given height.\n"
            "\nResult:\n"
            "x             (numeric) Hashes per second estimated\n"
            "\nExamples:\n" +
            HelpExampleCli("getnetworkhashps", "") +
            HelpExampleRpc("getnetworkhashps", ""));
    }

    LOCK(cs_main);
    return GetNetworkHashPS(
        !request.params[0].isNull() ? request.params[0].get_int() : 120,
        !request.params[1].isNull() ? request.params[1].get_int() : -1);
}

UniValue generateBlocks(const Config &config,
                        std::shared_ptr<CReserveScript> coinbaseScript,
                        int nGenerate, uint64_t nMaxTries, bool keepScript) {
    static const int nInnerLoopCount = 0x100000;
    int nHeightEnd = 0;
    int nHeight = 0;

    {
        // Don't keep cs_main locked.
        LOCK(cs_main);
        nHeight = ::ChainActive().Height();
        nHeightEnd = nHeight + nGenerate;
    }

    const uint64_t nExcessiveBlockSize = config.GetMaxBlockSize();

    unsigned int nExtraNonce = 0;
    UniValue blockHashes(UniValue::VARR);
    while (nHeight < nHeightEnd && !ShutdownRequested()) {
        std::unique_ptr<CBlockTemplate> pblocktemplate(
            BlockAssembler(config, g_mempool)
                .CreateNewBlock(coinbaseScript->reserveScript));

        if (!pblocktemplate.get()) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't create new block");
        }

        CBlock *pblock = &pblocktemplate->block;

        {
            LOCK(cs_main);
            IncrementExtraNonce(pblock, ::ChainActive().Tip(),
                                nExcessiveBlockSize, nExtraNonce);
        }

        while (nMaxTries > 0 && pblock->nNonce < nInnerLoopCount &&
               !CheckProofOfWork(pblock->GetHash(), pblock->nBits,
                                 config.GetChainParams().GetConsensus())) {
            ++pblock->nNonce;
            --nMaxTries;
        }

        if (nMaxTries == 0) {
            break;
        }

        if (pblock->nNonce == nInnerLoopCount) {
            continue;
        }

        std::shared_ptr<const CBlock> shared_pblock =
            std::make_shared<const CBlock>(*pblock);
        if (!ProcessNewBlock(config, shared_pblock, true, nullptr)) {
            throw JSONRPCError(RPC_INTERNAL_ERROR,
                               "ProcessNewBlock, block not accepted");
        }
        ++nHeight;
        blockHashes.push_back(pblock->GetHash().GetHex());

        // Mark script as important because it was used at least for one
        // coinbase output if the script came from the wallet.
        if (keepScript) {
            coinbaseScript->KeepScript();
        }
    }

    return blockHashes;
}

static UniValue generatetoaddress(const Config &config,
                                  const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 2 ||
        request.params.size() > 3) {
        throw std::runtime_error(
            "generatetoaddress nblocks address (maxtries)\n"
            "\nMine blocks immediately to a specified address (before the RPC "
            "call returns)\n"
            "\nArguments:\n"
            "1. nblocks      (numeric, required) How many blocks are generated "
            "immediately.\n"
            "2. address      (string, required) The address to send the newly "
            "generated bitcoin to.\n"
            "3. maxtries     (numeric, optional) How many iterations to try "
            "(default = 1000000).\n"
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks to myaddress\n" +
            HelpExampleCli("generatetoaddress", "11 \"myaddress\""));
    }

    int nGenerate = request.params[0].get_int();
    uint64_t nMaxTries = 1000000;
    if (!request.params[2].isNull()) {
        nMaxTries = request.params[2].get_int();
    }

    CTxDestination destination =
        DecodeDestination(request.params[1].get_str(), config.GetChainParams());
    if (!IsValidDestination(destination)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           "Error: Invalid address");
    }

    std::shared_ptr<CReserveScript> coinbaseScript =
        std::make_shared<CReserveScript>();
    coinbaseScript->reserveScript = GetScriptForDestination(destination);

    return generateBlocks(config, coinbaseScript, nGenerate, nMaxTries, false);
}

static UniValue getmininginfo(const Config &config,
                              const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "getmininginfo\n"
            "\nReturns a json object containing mining-related information."
            "\nResult:\n"
            "{\n"
            "  \"blocks\": nnn,             (numeric) The current block\n"
            "  \"currentblocksize\": nnn,   (numeric) The last block size\n"
            "  \"currentblocktx\": nnn,     (numeric) The last block "
            "transaction\n"
            "  \"difficulty\": xxx.xxxxx    (numeric) The current difficulty\n"
            "  \"networkhashps\": nnn,      (numeric) The network hashes per "
            "second\n"
            "  \"pooledtx\": n              (numeric) The size of the mempool\n"
            "  \"chain\": \"xxxx\",           (string) current network name as "
            "defined in BIP70 (main, test, regtest)\n"
            "  \"warnings\": \"...\"          (string) any network and "
            "blockchain warnings\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getmininginfo", "") +
            HelpExampleRpc("getmininginfo", ""));
    }

    LOCK(cs_main);

    UniValue obj(UniValue::VOBJ);
    obj.reserve(8);
    obj.pushKV("blocks", int(::ChainActive().Height()), false);
    obj.pushKV("currentblocksize", uint64_t(nLastBlockSize), false);
    obj.pushKV("currentblocktx", uint64_t(nLastBlockTx), false);
    obj.pushKV("difficulty", double(GetDifficulty(::ChainActive().Tip())), false);
    obj.pushKV("networkhashps", getnetworkhashps(config, request), false);
    obj.pushKV("pooledtx", uint64_t(g_mempool.size()), false);
    obj.pushKV("chain", config.GetChainParams().NetworkIDString(), false);
    obj.pushKV("warnings", GetWarnings("statusbar"), false);

    return obj;
}

// NOTE: Unlike wallet RPC (which use BCH values), mining RPCs follow GBT (BIP
// 22) in using satoshi amounts
static UniValue prioritisetransaction(const Config &config,
                                      const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 3) {
        throw std::runtime_error(
            "prioritisetransaction \"txid\" dummy fee_delta\n"
            "Accepts the transaction into mined blocks at a higher (or lower) "
            "priority\n"
            "\nArguments:\n"
            "1. \"txid\"       (string, required) The transaction id.\n"
            "2. dummy          (numeric, optional) API-Compatibility for "
            "previous API. Must be zero or null.\n"
            "                  DEPRECATED. For forward compatibility use named "
            "arguments and omit this parameter.\n"
            "3. fee_delta      (numeric, required) The fee value (in satoshis) "
            "to add (or subtract, if negative).\n"
            "                  The fee is not actually paid, only the "
            "algorithm for selecting transactions into a block\n"
            "                  considers the transaction as it would have paid "
            "a higher (or lower) fee.\n"
            "\nResult:\n"
            "true              (boolean) Returns true\n"
            "\nExamples:\n" +
            HelpExampleCli("prioritisetransaction", "\"txid\" 0.0 10000") +
            HelpExampleRpc("prioritisetransaction", "\"txid\", 0.0, 10000"));
    }

    LOCK(cs_main);

    TxId txid(ParseHashV(request.params[0], "txid"));
    Amount nAmount = request.params[2].get_int64() * SATOSHI;

    if (!(request.params[1].isNull() || request.params[1].get_real() == 0)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Priority is no longer supported, dummy argument to "
                           "prioritisetransaction must be 0.");
    }

    g_mempool.PrioritiseTransaction(txid, nAmount);
    return true;
}

// NOTE: Assumes a conclusive result; if result is inconclusive, it must be
// handled by caller
static UniValue BIP22ValidationResult(const Config &config,
                                      const CValidationState &state) {
    if (state.IsValid()) {
        return NullUniValue;
    }

    if (state.IsError()) {
        throw JSONRPCError(RPC_VERIFY_ERROR, FormatStateMessage(state));
    }

    if (state.IsInvalid()) {
        std::string strRejectReason = state.GetRejectReason();
        if (strRejectReason.empty()) {
            return "rejected";
        }
        return strRejectReason;
    }

    // Should be impossible.
    return "valid?";
}

/// Namespace for private data used by getblocktemplatelight and submitblocklight
namespace gbtl {
namespace {
// for use with unordered_map below
struct TrivialJobIdHasher {
    std::size_t operator()(const JobId &jobId) const noexcept {
        constexpr auto size = sizeof(std::size_t);
        static_assert(JobId::size() >= size, "sizeof(JobId) must be >= sizeof(size_t)");
        // this is faster than calling jobId.GetUint64(), especially for 32-bit.
        // note: we must use memcpy to guarantee aligned access.
        std::size_t ret;
        std::memcpy(&ret, jobId.begin(), size);
        return ret;
    }
};
/// Lock for the below two data structures
Mutex gJobIdMut;
/// Cache of transactions for block templates returned from getblocktemplatelight, used by submitblocklight
std::unordered_map<JobId, std::vector<CTransactionRef>, TrivialJobIdHasher> gJobIdTxCache GUARDED_BY(gJobIdMut);
/// This list allows us to implement an LRU cache. We remove items when this grows too large.
std::list<JobId> gJobIdList GUARDED_BY(gJobIdMut);

/// Bytes used as header and footer for the getblocktemplatelight data files we write out.
const std::string kDataFileMagic = "GBT";
} // namespace
} // namespace gbtl

static UniValue getblocktemplatecommon(bool fLight, const Config &config, const JSONRPCRequest &request) {
    const bool wrongParamSize = fLight ? request.params.size() > 2 : request.params.size() > 1;
    if (request.fHelp || wrongParamSize) {
        const std::string name = fLight ? "getblocktemplatelight" : "getblocktemplate";
        const auto lightExtraArgs = fLight ? " \"additional_txs\"" : "";
        const auto lightExtraArgDesc =
            fLight
            ? "2. additional_txs           (json array of strings, optional) Hex encoded transactions"
              " to add to the block (each tx must be unique and valid)\n"
            : "";
        const auto resultsKeysMiscDesc =
            fLight
            ? "  \"job_id\" : \"xxxx\",                (string) Job identifier as a hexadecimal hash160,"
              " which is to be used as a parameter in a subsequent call to submitblocklight\n"
              "  \"merkle\" : [ \"xxxx\", ... ],       (array) Hashes encoded in little-endian hexadecimal\n"
            : "  \"transactions\" : [                (array) contents of "
              "non-coinbase transactions that should be included in the next "
              "block\n"
              "      {\n"
              "         \"data\" : \"xxxx\",             (string) transaction "
              "data encoded in hexadecimal (byte-for-byte)\n"
              "         \"txid\" : \"xxxx\",             (string) transaction id "
              "encoded in little-endian hexadecimal\n"
              "         \"hash\" : \"xxxx\",             (string) hash encoded "
              "in little-endian hexadecimal\n"
              "         \"depends\" : [                (array) array of numbers"
              "\n"
              "             n                          (numeric) transactions "
              "before this one (by 1-based index in 'transactions' list) that "
              "must be present in the final block if this one is\n"
              "             ,...\n"
              "         ],\n"
              "         \"fee\": n,                    (numeric) difference in "
              "value between transaction inputs and outputs (in satoshis); for "
              "coinbase transactions, this is a negative number of the total "
              "collected block fees (i.e., not including the block subsidy); if "
              "key is not present, fee is unknown and clients MUST NOT assume "
              "there isn't one\n"
              "         \"sigops\" : n,                (numeric) total SigOps "
              "count, as counted for purposes of block limits; if key is not "
              "present, sigop count is unknown and clients MUST NOT assume it is "
              "zero\n"
              "         \"required\" : true|false      (boolean) if provided and "
              "true, this transaction must be in the final block\n"
              "      }\n"
              "      ,...\n"
              "  ],\n";

        throw std::runtime_error(
            name + " ( \"template_request\"" + lightExtraArgs +" )\n"
            "\nIf the request parameters include a 'mode' key, that is used to "
            "explicitly select between the default 'template' request or a "
            "'proposal'.\n"
            "It returns data needed to construct a block to work on.\n"
            "For full specification, see BIP22 and BIP23:\n"
            "    "
            "https://github.com/bitcoin/bips/blob/master/bip-0022.mediawiki\n"
            "    "
            "https://github.com/bitcoin/bips/blob/master/bip-0023.mediawiki\n"

            "\nArguments:\n"
            "1. template_request         (json object, optional) A json object "
            "in the following spec\n"
            "     {\n"
            "       \"mode\":\"template\"    (string, optional) This must be "
            "set to \"template\", \"proposal\" (see BIP23), or omitted\n"
            "       \"capabilities\":[     (array, optional) A list of "
            "strings\n"
            "           \"support\"          (string) client side supported "
            "feature, 'longpoll', 'coinbasetxn', 'coinbasevalue', 'proposal', "
            "'serverlist', 'workid'\n"
            "           ,...\n"
            "       ]\n"
            "     }\n"
            + lightExtraArgDesc +
            "\n"

            "\nResult:\n"
            "{\n"
            "  \"version\" : n,                    (numeric) The preferred "
            "block version\n"
            "  \"previousblockhash\" : \"xxxx\",     (string) The hash of "
            "current highest block\n"
            + resultsKeysMiscDesc +
            "  \"coinbaseaux\" : {                 (json object) data that "
            "should be included in the coinbase's scriptSig content\n"
            "      \"flags\" : \"xx\"                  (string) key name is to "
            "be ignored, and value included in scriptSig\n"
            "  },\n"
            "  \"coinbasevalue\" : n,              (numeric) maximum allowable "
            "input to coinbase transaction, including the generation award and "
            "transaction fees (in satoshis)\n"
            "  \"coinbasetxn\" : { ... },          (json object) information "
            "for coinbase transaction\n"
            "  \"target\" : \"xxxx\",                (string) The hash target\n"
            "  \"mintime\" : xxx,                  (numeric) The minimum "
            "timestamp appropriate for next block time in seconds since epoch "
            "(Jan 1 1970 GMT)\n"
            "  \"mutable\" : [                     (array of string) list of "
            "ways the block template may be changed\n"
            "     \"value\"                          (string) A way the block "
            "template may be changed, e.g. 'time', 'transactions', "
            "'prevblock'\n"
            "     ,...\n"
            "  ],\n"
            "  \"noncerange\" : \"00000000ffffffff\",(string) A range of valid "
            "nonces\n"
            "  \"sigoplimit\" : n,                 (numeric) limit of sigchecks "
            "in blocks\n"
            "  \"sizelimit\" : n,                  (numeric) limit of block "
            "size\n"
            "  \"curtime\" : ttt,                  (numeric) current timestamp "
            "in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"bits\" : \"xxxxxxxx\",              (string) compressed "
            "target of next block\n"
            "  \"height\" : n                      (numeric) The height of the "
            "next block\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli(name, "") +
            HelpExampleRpc(name, ""));
    }
    const auto t0 = GetTimeMicros(); // perf. info, used iff logging BCLog::RPC
    LOCK(cs_main);

    std::string strMode = "template";
    const UniValue *lpval = &NullUniValue;
    std::set<std::string> setClientRules;
    if (!request.params[0].isNull()) {
        const UniValue &oparam = request.params[0].get_obj();
        const UniValue &modeval = oparam["mode"];
        if (modeval.isStr()) {
            strMode = modeval.get_str();
        } else if (modeval.isNull()) {
            /* Do nothing */
        } else {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");
        }
        lpval = &oparam["longpollid"];

        if (strMode == "proposal") {
            const UniValue &dataval = oparam["data"];
            if (!dataval.isStr()) {
                throw JSONRPCError(RPC_TYPE_ERROR,
                                   "Missing data String key for proposal");
            }

            CBlock block;
            if (!DecodeHexBlk(block, dataval.get_str())) {
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                                   "Block decode failed");
            }

            const BlockHash hash = block.GetHash();
            const CBlockIndex *pindex = LookupBlockIndex(hash);
            if (pindex) {
                if (pindex->IsValid(BlockValidity::SCRIPTS)) {
                    return "duplicate";
                }
                if (pindex->nStatus.isInvalid()) {
                    return "duplicate-invalid";
                }
                return "duplicate-inconclusive";
            }

            CBlockIndex *const pindexPrev = ::ChainActive().Tip();
            // TestBlockValidity only supports blocks built on the current Tip
            if (block.hashPrevBlock != pindexPrev->GetBlockHash()) {
                return "inconclusive-not-best-prevblk";
            }
            CValidationState state;
            TestBlockValidity(state, config.GetChainParams(), block, pindexPrev,
                              BlockValidationOptions(config)
                                  .withCheckPoW(false)
                                  .withCheckMerkleRoot(true));
            return BIP22ValidationResult(config, state);
        }
    }

    if (strMode != "template") {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");
    }

    if (!g_connman) {
        throw JSONRPCError(
            RPC_CLIENT_P2P_DISABLED,
            "Error: Peer-to-peer functionality missing or disabled");
    }

    if (g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0) {
        throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED,
                           "Bitcoin is not connected!");
    }

    if (IsInitialBlockDownload()) {
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD,
                           "Bitcoin is downloading blocks...");
    }

    static unsigned int nTransactionsUpdatedLast;

    if (!lpval->isNull()) {
        // Wait to respond until either the best block changes, OR a minute has
        // passed and there are more transactions
        uint256 hashWatchedChain;
        std::chrono::steady_clock::time_point checktxtime;
        unsigned int nTransactionsUpdatedLastLP;

        if (lpval->isStr()) {
            // Format: <hashBestChain><nTransactionsUpdatedLast>
            const std::string &lpstr = lpval->get_str();

            hashWatchedChain = ParseHashV(lpstr.substr(0, 64), "longpollid");
            nTransactionsUpdatedLastLP = atoi64(lpstr.substr(64));
        } else {
            // NOTE: Spec does not specify behaviour for non-string longpollid,
            // but this makes testing easier
            hashWatchedChain = ::ChainActive().Tip()->GetBlockHash();
            nTransactionsUpdatedLastLP = nTransactionsUpdatedLast;
        }

        // Release the wallet and main lock while waiting
        LEAVE_CRITICAL_SECTION(cs_main);
        {
            checktxtime =
                std::chrono::steady_clock::now() + std::chrono::minutes(1);

            WAIT_LOCK(g_best_block_mutex, lock);
            while (g_best_block == hashWatchedChain && IsRPCRunning()) {
                if (g_best_block_cv.wait_until(lock, checktxtime) ==
                    std::cv_status::timeout) {
                    // Timeout: Check transactions for update
                    if (g_mempool.GetTransactionsUpdated() !=
                        nTransactionsUpdatedLastLP) {
                        break;
                    }
                    checktxtime += std::chrono::seconds(10);
                }
            }
        }
        ENTER_CRITICAL_SECTION(cs_main);

        if (!IsRPCRunning()) {
            throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Shutting down");
        }
        // TODO: Maybe recheck connections/IBD and (if something wrong) send an
        // expires-immediately template to stop miners?
    }

    struct LightResult {
        const gbtl::JobId jobId;
        const UniValue merkle;
    };
    // Update block
    static CBlockIndex *pindexPrev;
    static int64_t nStart;
    static std::unique_ptr<CBlockTemplate> pblocktemplate;
    static std::unique_ptr<LightResult> plightresult; // fLight mode only, cached result associated with pblocktemplate
    if (pindexPrev != ::ChainActive().Tip() ||
        (g_mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast &&
         GetTime() - nStart > 5)) {
        // Clear pindexPrev so future calls make a new block, despite any
        // failures from here on
        pindexPrev = nullptr;

        // Store the pindexBest used before CreateNewBlock, to avoid races
        nTransactionsUpdatedLast = g_mempool.GetTransactionsUpdated();
        CBlockIndex *pindexPrevNew = ::ChainActive().Tip();
        nStart = GetTime();

        // Create new block
        CScript scriptDummy = CScript() << OP_TRUE;
        pblocktemplate =
            BlockAssembler(config, g_mempool).CreateNewBlock(scriptDummy);
        plightresult.reset();
        if (!pblocktemplate) {
            throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");
        }

        // Need to update only after we know CreateNewBlock succeeded
        pindexPrev = pindexPrevNew;
    }

    assert(pindexPrev);
    // pointer for convenience
    CBlock *pblock = &pblocktemplate->block;

    // Temporary tx vector which is only used if we are in fLight mode and we have `additional_txs` specified by the
    // client. This vector is the set of tx's in pblock *plus* `additional_txs` for this invocation.
    // We use a std::unique_ptr here because it's lighter weight to construct than an empty std::vector would be. (The
    // common case here is that this vector is unused, since `additional_txs` is an advanced feature).
    using TxRefVector = std::vector<CTransactionRef>;
    std::unique_ptr<TxRefVector> tmpBlockTxsWithAdditionalTxs;
    // Below normally points to pblock->vtx -- *unless* we are in fLight mode and we have "additional_txs", in which
    // case it points to the above vector (which will be made valid in that case).
    TxRefVector *pvtx = &pblock->vtx;

    if (fLight && request.params.size() > 1) {
        // GBTLight only -- append transactions to be added from `additional_txs`. Note the contract with client code is
        // these must be valid and unique. As a performance-saving measure we do not check them for validity or
        // uniqueness.
        //
        // Also note: We do *NOT* append additional_txs to pblock! This would mean additional_txs get added each time
        // we are called.  However, pblock sticks around as global state for 5 seconds. `additional_txs` is a
        // per-invocation parameter, and should not affect global state.  So, instead, we must use the `pvtx` pointer
        // above which will end up pointing to a private copy of pblock's tx data which we will create below.
        const UniValue &injectedTxsHex = request.params[1].get_array(); // throws error to client if not array
        const auto size = injectedTxsHex.size();
        if (size) {
            // copy pblock's txs since we need a private copy to modify the set of txs for this invocation
            tmpBlockTxsWithAdditionalTxs = std::make_unique<TxRefVector>();
            pvtx = tmpBlockTxsWithAdditionalTxs.get();
            pvtx->reserve(pblock->vtx.size() + size); // final size is template tx's + additional_txs
            pvtx->insert(pvtx->end(), pblock->vtx.begin(), pblock->vtx.end());
            // next, inject additional_txs
            for (size_t idx = 0; idx < size; ++idx) {
                CMutableTransaction tx;
                if (DecodeHexTx(tx, injectedTxsHex[idx].get_str())) {
                    pvtx->push_back(MakeTransactionRef(std::move(tx)));
                } else {
                    throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                                       strprintf("additional_txs transaction %d decode failure", idx));
                }
            }
            LogPrint(BCLog::RPC, "added %d additional_txs to block template\n", size);
            const Consensus::Params &consensusParams = config.GetChainParams().GetConsensus();
            if (IsMagneticAnomalyEnabled(consensusParams, pindexPrev)) {
                auto start = pvtx->front()->IsCoinBase() ? std::next(pvtx->begin()) : pvtx->begin();
                std::sort(start, pvtx->end(),
                          [](const CTransactionRef &a, const CTransactionRef &b) -> bool {
                              return a->GetId() < b->GetId();
                          });
            }
            // From this point forward the fLight code below must operate on the pvtx (private copy) of the tx set,
            // and not pblock->vtx.
        }
    }

    // Update nTime
    UpdateTime(pblock, config.GetChainParams().GetConsensus(), pindexPrev);
    pblock->nNonce = 0;

    UniValue aCaps(UniValue::VARR);
    aCaps.push_back("proposal");

    uint160 jobId; // fLight version only, ends up in results["job_id"]
    UniValue merkle(UniValue::VARR); // fLight version only, ends up in results["merkle"]
    UniValue transactions(UniValue::VARR); // !fLight version only, ends up in results["transactions"]
    if (fLight) {
        assert(pvtx && (!tmpBlockTxsWithAdditionalTxs || pvtx == tmpBlockTxsWithAdditionalTxs.get()));
        if (plightresult && pvtx == &pblock->vtx) {
            // use cached light result since we know it's for this tx set (no additional_txs and result is valid)
            LogPrint(BCLog::RPC, "Using cached merkle result\n");
            jobId = plightresult->jobId; // copy jobId
            merkle = plightresult->merkle; // copy UniValue
        } else {
            // merkle cached result not available (new template) or we have additional_txs and can't use cached result
            LogPrint(BCLog::RPC, "Calculating new merkle result\n");
            std::vector<uint256> vtxIdsNoCoinbase; // txs without coinbase
            // we reserve 1 more than we need, because makeMerkleBranch may use a little more space
            vtxIdsNoCoinbase.reserve(pvtx->size());
            for (const auto &tx : *pvtx) {
                if (tx->IsCoinBase())
                    continue;
                vtxIdsNoCoinbase.push_back(tx->GetId());
            }
            // make merkleSteps and merkle branch
            const auto merkleSteps = gbtl::MakeMerkleBranch(std::move(vtxIdsNoCoinbase));
            merkle.reserve(merkleSteps.size());
            // hash source is Hash160(hashPrevBlock + concatenation_of_all_merkle_step_hashes)
            std::vector<uint8_t> hashSource;
            hashSource.reserve(pblock->hashPrevBlock.size() + merkleSteps.size()*32);
            hashSource.insert(hashSource.end(), pblock->hashPrevBlock.begin(), pblock->hashPrevBlock.end());
            for (const auto &h : merkleSteps) {
                merkle.push_back(h.GetHex()); // push UniValue
                hashSource.insert(hashSource.end(), h.begin(), h.end()); // add to hash source
            }
            // Compute the jobId -- we will return this jobId to the client and also generate a cache entry based on it
            // towards the end of this function.
            jobId = Hash160(hashSource.begin(), hashSource.end());

            // Finally, cache the merkle results if they were calculated from the tx's in pblock (no additional_txs).
            if (pvtx == &pblock->vtx) {
                plightresult.reset(new LightResult{jobId, merkle});
                LogPrint(BCLog::RPC, "Saved merkle result\n");
            }
        }
    } else {
        // regular (!fLight) mode
        transactions.reserve(pblock->vtx.size());
        int index_in_template = 0;
        for (const auto &it : pblock->vtx) {
            const CTransaction &tx = *it;

            if (tx.IsCoinBase()) {
                index_in_template++;
                continue;
            }

            UniValue entry(UniValue::VOBJ);
            entry.pushKV("data", EncodeHexTx(tx), false);
            entry.pushKV("txid", tx.GetId().GetHex(), false);
            entry.pushKV("hash", tx.GetHash().GetHex(), false);
            entry.pushKV("fee", pblocktemplate->entries[index_in_template].fees / SATOSHI, false);
            int64_t nTxSigOps = pblocktemplate->entries[index_in_template].sigOpCount;
            entry.pushKV("sigops", nTxSigOps, false);

            transactions.push_back(std::move(entry));
            index_in_template++;
        }
    }

    UniValue aux(UniValue::VOBJ);
    aux.pushKV("flags", HexStr(COINBASE_FLAGS.begin(), COINBASE_FLAGS.end()));

    arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);

    UniValue aMutable(UniValue::VARR);
    aMutable.push_back("time");
    if (!fLight) {
        aMutable.push_back("transactions");
    } else {
        aMutable.push_back("job_id");
        aMutable.push_back("merkle");
    }
    aMutable.push_back("prevblock");

    UniValue result(UniValue::VOBJ);
    result.pushKV("capabilities", std::move(aCaps), false);

    result.pushKV("version", pblock->nVersion, false);

    result.pushKV("previousblockhash", pblock->hashPrevBlock.GetHex(), false);
    if (!fLight) {
        result.pushKV("transactions", std::move(transactions), false);
    } else {
        result.pushKV("job_id", jobId.GetHex(), false);
        result.pushKV("merkle", std::move(merkle), false);
    }
    result.pushKV("coinbaseaux", std::move(aux), false);
    result.pushKV("coinbasevalue", int64_t(pblock->vtx[0]->vout[0].nValue / SATOSHI), false);
    result.pushKV("longpollid",
                  ::ChainActive().Tip()->GetBlockHash().GetHex() + i64tostr(nTransactionsUpdatedLast),
                  false);
    result.pushKV("target", hashTarget.GetHex(), false);
    result.pushKV("mintime", int64_t(pindexPrev->GetMedianTimePast()) + 1, false);
    result.pushKV("mutable", std::move(aMutable), false);
    result.pushKV("noncerange", "00000000ffffffff", false);
    result.pushKV("sigoplimit", GetMaxBlockSigChecksCount(DEFAULT_MAX_BLOCK_SIZE), false);
    result.pushKV("sizelimit", DEFAULT_MAX_BLOCK_SIZE, false);
    result.pushKV("curtime", pblock->GetBlockTime(), false);
    result.pushKV("bits", strprintf("%08x", pblock->nBits), false);
    result.pushKV("height", int64_t(pindexPrev->nHeight) + 1, false);

    if (fLight) {
        // Note: this must be called with cs_main held (which is the case here)
        gbtl::CacheAndSaveTxsToFile(jobId, pvtx);
    }

    LogPrint(BCLog::RPC, "getblocktemplatecommon: took %f secs\n", (GetTimeMicros() - t0) / 1e6);
    return result;
}

static UniValue getblocktemplate(const Config &config, const JSONRPCRequest &request) {
    return getblocktemplatecommon(false, config, request);
}

static UniValue getblocktemplatelight(const Config &config, const JSONRPCRequest &request) {
    return getblocktemplatecommon(true, config, request);
}

class submitblock_StateCatcher : public CValidationInterface {
public:
    uint256 hash;
    bool found;
    CValidationState state;

    explicit submitblock_StateCatcher(const uint256 &hashIn)
        : hash(hashIn), found(false), state() {}

protected:
    void BlockChecked(const CBlock &block,
                      const CValidationState &stateIn) override {
        if (block.GetHash() != hash) {
            return;
        }

        found = true;
        state = stateIn;
    }
};

/// If jobId is not nullptr, we are in `submitblocklight` mode, otherwise we are in regular `submitblock` mode.
static UniValue submitblockcommon(const Config &config, const JSONRPCRequest &request,
                                  const gbtl::JobId *jobId = nullptr) {
    const auto t0 = GetTimeMicros(); // perf. logging, only used if BCLog::RPC

    std::shared_ptr<CBlock> blockptr = std::make_shared<CBlock>();
    CBlock &block = *blockptr;
    if (!DecodeHexBlk(block, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");
    }

    auto tDeserTx = jobId ? GetTimeMicros() : 0; // for perf. logging iff BCLog::RPC is enabled

    if (jobId) {
        // submitblocklight version, client just sends header + coinbase and we add the rest
        // of the tx's from a tx cache created previously in getblocktemplatelight
        if (block.vtx.size() != 1 || !block.vtx[0]->IsCoinBase()) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block must contain a single coinbase tx (light version)");
        }

        if (!gbtl::GetTxsFromCache(*jobId, block)) {
            // not found in in-memory cache, try our getblocktemplatelight job file cache
            gbtl::LoadTxsFromFile(*jobId, block); // throws on failure
        }
        tDeserTx = GetTimeMicros() - tDeserTx; // perf.
    } else {
        // regular version -- requires full block from client
        if (block.vtx.empty() || !block.vtx[0]->IsCoinBase()) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block does not start with a coinbase");
        }
    }

    const BlockHash hash = block.GetHash();
    {
        LOCK(cs_main);
        const CBlockIndex *pindex = LookupBlockIndex(hash);
        if (pindex) {
            if (pindex->IsValid(BlockValidity::SCRIPTS)) {
                return "duplicate";
            }
            if (pindex->nStatus.isInvalid()) {
                return "duplicate-invalid";
            }
        }
    }

    bool new_block;
    submitblock_StateCatcher sc(block.GetHash());
    RegisterValidationInterface(&sc);
    bool accepted =
        ProcessNewBlock(config, blockptr, /* fForceProcessing */ true, /* fNewBlock */ &new_block);
    // We are only interested in BlockChecked which will have been dispatched
    // in-thread, so no need to sync before unregistering.
    UnregisterValidationInterface(&sc);
    // Sync to ensure that the catcher's slots aren't executing when it goes out
    // of scope and is deleted.
    SyncWithValidationInterfaceQueue();
    if (!new_block && accepted) {
        return "duplicate";
    }

    if (!sc.found) {
        return "inconclusive";
    }

    const auto result = BIP22ValidationResult(config, sc.state);
    if (jobId) {
        LogPrint(BCLog::RPC, "SubmitBlock (light) deserialize duration: %f seconds\n", tDeserTx / 1e6);
    }
    LogPrint(BCLog::RPC, "SubmitBlock total duration: %f seconds\n", (GetTimeMicros() - t0) / 1e6);
    return result;
}

static UniValue submitblock(const Config &config, const JSONRPCRequest &request) {
    // We allow 2 arguments for compliance with BIP22. Argument 2 is ignored.
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            "submitblock \"hexdata\"  ( \"dummy\" )\n"
            "\nAttempts to submit new block to network.\n"
            "See https://en.bitcoin.it/wiki/BIP_0022 for full specification.\n"

            "\nArguments\n"
            "1. \"hexdata\"        (string, required) the hex-encoded block "
            "data to submit\n"
            "2. \"dummy\"          (optional) dummy value, for compatibility "
            "with BIP22. This value is ignored.\n"
            "\nResult:\n"
            "\nExamples:\n" +
            HelpExampleCli("submitblock", "\"mydata\"") +
            HelpExampleRpc("submitblock", "\"mydata\""));
    }
    return submitblockcommon(config, request);
}

static UniValue submitblocklight(const Config &config, const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            "submitblocklight \"hexdata\" \"job_id\"\n"
            "\nAttempts to submit a new block to the network, based on a previous call to getblocktemplatelight.\n"

            "\nArguments\n"
            "1. \"hexdata\"        (string, required) The hex-encoded block data to submit. The block must have "
            "exactly 1 transaction (coinbase). Additional transactions (if any) are appended from the light template.\n"
            "2. \"job_id\"         (string, required) Identifier of the light template from which to retrieve the "
            "non-coinbase transactions. This job_id must be obtained from a previous call to getblocktemplatelight.\n"
            "\nResult:\n"
            "\nExamples:\n" +
            HelpExampleCli("submitblocklight", "\"mydata\" \"myjobid\"") +
            HelpExampleRpc("submitblocklight", "\"mydata\", \"myjobid\""));
    }

    const auto jobIdStr = request.params[1].get_str();
    gbtl::JobId jobId; // uint160
    if (!ParseHashStr(jobIdStr, jobId)) {
         throw std::runtime_error("job_id must be a 40 character hexadecimal string (not '" + jobIdStr + "')");
    }
    return submitblockcommon(config, request, &jobId);
}

static UniValue submitheader(const Config &config,
                             const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error("submitheader \"hexdata\"\n"
                                 "\nDecode the given hexdata as a header and "
                                 "submit it as a candidate chain tip if valid."
                                 "\nThrows when the header is invalid.\n"
                                 "\nArguments\n"
                                 "1. \"hexdata\"        (string, required) the "
                                 "hex-encoded block header data\n"
                                 "\nResult:\n"
                                 "None"
                                 "\nExamples:\n" +
                                 HelpExampleCli("submitheader", "\"aabbcc\"") +
                                 HelpExampleRpc("submitheader", "\"aabbcc\""));
    }

    CBlockHeader h;
    if (!DecodeHexBlockHeader(h, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                           "Block header decode failed");
    }
    {
        LOCK(cs_main);
        if (!LookupBlockIndex(h.hashPrevBlock)) {
            throw JSONRPCError(RPC_VERIFY_ERROR,
                               "Must submit previous header (" +
                                   h.hashPrevBlock.GetHex() + ") first");
        }
    }

    CValidationState state;
    ProcessNewBlockHeaders(config, {h}, state, /* ppindex */ nullptr,
                           /* first_invalid */ nullptr);
    if (state.IsValid()) {
        return NullUniValue;
    }
    if (state.IsError()) {
        throw JSONRPCError(RPC_VERIFY_ERROR, FormatStateMessage(state));
    }
    throw JSONRPCError(RPC_VERIFY_ERROR, state.GetRejectReason());
}

static UniValue estimatefee(const Config &config,
                            const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() > 0) {
        throw std::runtime_error(
            "estimatefee\n"
            "\nEstimates the approximate fee per kilobyte needed for a "
            "transaction\n"
            "\nResult:\n"
            "n              (numeric) estimated fee-per-kilobyte\n"
            "\nExample:\n" +
            HelpExampleCli("estimatefee", ""));
    }

    return ValueFromAmount(g_mempool.estimateFee().GetFeePerK());
}

namespace gbtl {

std::vector<uint256> MakeMerkleBranch(std::vector<uint256> hashes) {
    /*   This algorithm returns a merkle path suitable for reconstructing the merkle root of a block given a set of
         txhashes and an unknown coinbase tx (a coinbase tx to be determined by the miner in the future).

         Key: tx    = txid (leaf),
              ?cb   = unknown coinbase (leaf),
              m     = known internal merkle (not returned, but used internally for calculation),
              ?     = unknown merkle (depends on not-yet-determined coinbase)
              0,1,2 = The numbers below are the array positions of the returned 'steps' array.

                        ?  <-- unknown root (calculated by miner when coinbase tx is decided, given returned "steps")
                        /\
                      /    \
                    /        \
                  /            \
                 ?               2   <-- known "step" (returned)
               /   \           /   \
              ?      1        m      m  <-- known merkle (computed in-place, not returned)
             / \    / \      / \    / \
            |   |  |   |    |   |  |   |
           ?cb  0  tx  tx   tx  tx tx  tx   <-- input hashes (txids)
    */
    std::vector<uint256> steps;
    if (hashes.empty())
        return steps;
    steps.reserve(size_t(std::ceil(std::log2(hashes.size() + 1)))); // results will be of size ~log2 input hashes
    while (hashes.size() > 1) {
        // put first element
        steps.push_back(hashes.front());
        if (!(hashes.size() & 0x1)) {
            // If size is even, push_back the end again, size should be an odd number.
            // (because we ignore the coinbase tx when computing the merkle branch)
            hashes.push_back(hashes.back());
        }
        // ignore the first one then merge two
        const size_t reducedSize = (hashes.size() - 1) / 2;
        for (size_t i = 0; i < reducedSize; ++i) {
            // Hash = Double SHA256.
            // The below SHA256D64 call is equivalent to this call, except it hashes in-place, so it's faster.
            //hashes[i] = Hash(hashes[i * 2 + 1].begin(), hashes[i * 2 + 1].end(),
            //                 hashes[i * 2 + 2].begin(), hashes[i * 2 + 2].end());
            SHA256D64(hashes[i].begin(), hashes[i * 2 + 1].begin(), 1);
        }
        hashes.resize(reducedSize);
  }
  assert(hashes.size() == 1);
  steps.push_back(hashes.front()); // put the last one
  return steps;
}

bool GetTxsFromCache(const JobId &jobId, CBlock &block) {
    LOCK(gJobIdMut);
    const auto it = gJobIdTxCache.find(jobId);
    if (it != gJobIdTxCache.end()) {
        // found!  Add to block
        const auto & vtx = it->second;
        block.vtx.insert(block.vtx.end(), vtx.begin(), vtx.end());
        return true;
    }
    return false;
}

void LoadTxsFromFile(const JobId &jobId, CBlock &block) {
    static const std::string errNoData{"job_id data not available"},
                             errDataEmpty{"job_id data is empty"},
                             errDataBad{"job_id data is invalid"};
    const auto jobIdStr = jobId.GetHex();
    fs::path filename = GetJobDataDir() / jobIdStr;
    if (!fs::exists(filename)) {
        LogPrintf("WARNING: SubmitBlockLight cannot find file for job_id %s, searching trash dir\n", jobIdStr);
        filename = GetJobDataTrashDir() / jobIdStr;
        if (!fs::exists(filename)) {
            LogPrintf("WARNING: SubmitBlockLight cannot find file for job_id %s in trash dir either, giving up\n", jobIdStr);
            throw JSONRPCError(RPC_INVALID_PARAMETER, errNoData);
        }
    }
    LogPrint(BCLog::RPC, "SubmitBlockLight job_id %s found in %s\n", jobIdStr, filename.string());
    try {
        const auto magicLen = kDataFileMagic.size(); // 3 for "GBT"
        std::vector<uint8_t> dataBuf;
        {
            fs::ifstream file(filename);

            if (!file.is_open()) {
                LogPrintf("WARNING: SubmitBlockLight found file for job_id %s, but open failed\n", jobIdStr);
                throw JSONRPCError(RPC_INTERNAL_ERROR, errNoData);
            }

            std::streamoff fileSize{};
            if (!file.seekg(0, file.end) || (fileSize = file.tellg()) < std::streamoff(magicLen * 2 + 1)) {
                LogPrintf("WARNING: SubmitBlockLight job_id %s has invalid size\n", jobIdStr);
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, errDataEmpty);
            }
            file.seekg(0, file.beg);
            dataBuf.resize(size_t(fileSize));
            char * const charDataBuf = reinterpret_cast<char *>(dataBuf.data());
            file.read(charDataBuf, fileSize);
            // check read was good and that header and footer match
            if (file.fail()
                    // header must match "GBT"
                    || 0 != std::memcmp(charDataBuf, kDataFileMagic.data(), magicLen)
                    // footer must match "GBT"
                    || 0 != std::memcmp(charDataBuf + fileSize - magicLen, kDataFileMagic.data(), magicLen)) {
                LogPrintf("WARNING: SubmitBlockLight job_id %s appears to be corrupt\n", jobIdStr);
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, errDataBad);
            }
            // everything's ok, proceed
        }
        // deserialize the vector directly, starting at pos 3 (after "GBT" header)
        VectorReader vr(SER_NETWORK, PROTOCOL_VERSION, dataBuf, magicLen /* start pos */);
        uint32_t txCount = 0;
        vr >> txCount;
        for (uint32_t i = 0; i < txCount; ++i) {
            CMutableTransaction mutableTx;
            vr >> mutableTx;
            block.vtx.push_back(MakeTransactionRef(std::move(mutableTx)));
        }
    } catch (const std::exception & e) {
        // Note: JSONRPCError() above throws a UniValue so it will not be caught here (but it will
        // propagate out anyway to the client). This clause is for low-level std::ios_base::failure
        // and potentially even std::bad_alloc.
        LogPrintf("WARNING: SubmitBlockLight job_id %s failed to deserialize: %s\n", jobIdStr, e.what());
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, errDataBad);
    }
}

void CacheAndSaveTxsToFile(const JobId &jobId, const std::vector<CTransactionRef> *pvtx) {
    std::vector<CTransactionRef> storeTxs;
    bool didSetupStoreTxs = false;
    const auto setupStoreTxs = [&storeTxs, &didSetupStoreTxs, pvtx] {
        if (didSetupStoreTxs)
            return;
        if (!pvtx->empty()) {
            // we store all but the first tx (all but coinbase)
            auto start = pvtx->front()->IsCoinBase() ? std::next(pvtx->begin()) : pvtx->begin();
            storeTxs.insert(storeTxs.end(), start, pvtx->end());
        }
        didSetupStoreTxs = true;
    };
    // first, write to file if file does not already exist
    {
        const auto jobIdStr = jobId.GetHex();
        fs::path outputFile = GetJobDataDir() / jobIdStr;
        // There is no race condition here because this function is called from getblocktemplatecommon,
        // with the cs_main lock held.
        AssertLockHeld(cs_main);
        if (!fs::exists(outputFile)) {
            setupStoreTxs();
            CDataStream datastream(SER_NETWORK, PROTOCOL_VERSION);
            const uint32_t nTx = uint32_t(storeTxs.size());
            datastream.reserve(256 * nTx + sizeof(nTx)); // assume avg 256 byte tx size. This doesn't have to be exact, this is just to avoid redundant allocations as we serialize.
            datastream << nTx; // first write the size
            for (const auto &txRef : storeTxs)
                datastream << *txRef;

            const auto t0 = GetTimeMicros(); // for perf. logging iff BCLog::RPC is enabled
            auto tmpOut = outputFile;
            tmpOut += gbtl::tmpExt; // += ".tmp"
            bool ok{};
            {
                fs::ofstream ofile(tmpOut, std::ios_base::binary|std::ios_base::out|std::ios_base::trunc);
                if ((ok = ofile.is_open())) {
                    // "GBT" magic bytes at front
                    using std::streamsize;
                    ofile.write(kDataFileMagic.data(), streamsize(kDataFileMagic.size()));
                    if (ofile)
                        ofile.write(datastream.data(), streamsize(datastream.size()));
                    if (ofile)
                        // "GBT" magic bytes at end
                        ofile.write(kDataFileMagic.data(), streamsize(kDataFileMagic.size()));
                    ok = bool(ofile);
                }
            } // file is closed
            if (ok) {
                // now, atomically move it in place.
                fs::rename(tmpOut, outputFile); // may throw (unlikely) iff another prog. created outputFile just now
                LogPrint(BCLog::RPC, "getblocktemplatelight: %d txs written to %s in %f secs\n", storeTxs.size(),
                         outputFile.string(), (GetTimeMicros() - t0) / 1e6);
            } else {
                LogPrintf("getblocktemplatelight: cannot write tx data to %s\n", tmpOut.string());
                try { fs::remove(tmpOut); } catch (...) {}
                // We must throw here. Clients should be alerted that there is a misconfiguration with bitcoind (even
                // though we could theoretically continue and rely on in-memory cache, we are better off doing this).
                throw JSONRPCError(RPC_INTERNAL_ERROR, "failed to save job tx data to disk");
            }
        }
    }
    // lastly, store cache if not already in cache
    {
        // we must hold this lock here since this data is shared with submitblocklight which doesn't hold cs_main
        LOCK(gJobIdMut);
        if (gJobIdTxCache.find(jobId) == gJobIdTxCache.end()) {
            setupStoreTxs();
            // put in cache, but first check size to limit cache size
            if (gJobIdTxCache.size() >= GetJobCacheSize() && !gJobIdList.empty()) {
                // remove the oldest jobId
                const auto & oldJobId = gJobIdList.front();
                LogPrint(BCLog::RPC, "getblocktemplatelight: in-memory cache full, old job_id %s removed\n",
                         oldJobId.GetHex());
                gJobIdTxCache.erase(oldJobId);
                gJobIdList.pop_front();
            }
            // we insert using emplace and forward_as_tuple for minimal overhead
            gJobIdTxCache.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(jobId), std::forward_as_tuple(std::move(storeTxs)));
            // NB: at this point storeTxs is empty (moved)
            gJobIdList.push_back(jobId);
        }
    }
}

} // namespace gbtl

// clang-format off
static const ContextFreeRPCCommand commands[] = {
    //  category   name                     actor (function)       argNames
    //  ---------- ------------------------ ---------------------- ----------
    {"mining",     "getnetworkhashps",      getnetworkhashps,      {"nblocks", "height"}},
    {"mining",     "getmininginfo",         getmininginfo,         {}},
    {"mining",     "prioritisetransaction", prioritisetransaction, {"txid", "dummy", "fee_delta"}},
    {"mining",     "getblocktemplate",      getblocktemplate,      {"template_request"}},
    {"mining",     "getblocktemplatelight", getblocktemplatelight, {"template_request", "additional_txs"}},
    {"mining",     "submitblock",           submitblock,           {"hexdata", "parameters"}},
    {"mining",     "submitblocklight",      submitblocklight,      {"hexdata", "job_id"}},
    {"mining",     "submitheader",          submitheader,          {"hexdata"}},

    {"generating", "generatetoaddress",     generatetoaddress,     {"nblocks", "address", "maxtries"}},

    {"util",       "estimatefee",           estimatefee,           {"nblocks"}},
};
// clang-format on

void RegisterMiningRPCCommands(CRPCTable &t) {
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++) {
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
    }
}
