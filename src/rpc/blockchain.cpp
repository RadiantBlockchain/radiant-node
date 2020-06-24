// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/blockchain.h>

#include <amount.h>
#include <chain.h>
#include <chainparams.h>
#include <checkpoints.h>
#include <coins.h>
#include <config.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <hash.h>
#include <index/txindex.h>
#include <key_io.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <script/descriptor.h>
#include <streams.h>
#include <sync.h>
#include <txdb.h>
#include <txmempool.h>
#include <undo.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <validation.h>
#include <validationinterface.h>
#include <warnings.h>

#include <boost/thread/thread.hpp> // boost::thread::interrupt

#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

struct CUpdatedBlock {
    uint256 hash;
    int height;
};

static Mutex cs_blockchange;
static std::condition_variable cond_blockchange;
static CUpdatedBlock latestblock;

/**
 * Calculate the difficulty for a given block index.
 */
double GetDifficulty(const CBlockIndex *blockindex) {
    assert(blockindex);

    int nShift = (blockindex->nBits >> 24) & 0xff;
    double dDiff = double(0x0000ffff) / double(blockindex->nBits & 0x00ffffff);

    while (nShift < 29) {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29) {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

static int ComputeNextBlockAndDepth(const CBlockIndex *tip,
                                    const CBlockIndex *blockindex,
                                    const CBlockIndex *&next) {
    next = tip->GetAncestor(blockindex->nHeight + 1);
    if (next && next->pprev == blockindex) {
        return tip->nHeight - blockindex->nHeight + 1;
    }
    next = nullptr;
    return blockindex == tip ? 1 : -1;
}

UniValue::Object blockheaderToJSON(const CBlockIndex *tip, const CBlockIndex *blockindex) {
    const CBlockIndex *pnext;
    int confirmations = ComputeNextBlockAndDepth(tip, blockindex, pnext);
    bool previousblockhash = blockindex->pprev;
    bool nextblockhash = pnext;
    UniValue::Object result;
    result.reserve(13 + previousblockhash + nextblockhash);
    result.emplace_back("hash", blockindex->GetBlockHash().GetHex());
    result.emplace_back("confirmations", confirmations);
    result.emplace_back("height", blockindex->nHeight);
    result.emplace_back("version", blockindex->nVersion);
    result.emplace_back("versionHex", strprintf("%08x", blockindex->nVersion));
    result.emplace_back("merkleroot", blockindex->hashMerkleRoot.GetHex());
    result.emplace_back("time", blockindex->nTime);
    result.emplace_back("mediantime", blockindex->GetMedianTimePast());
    result.emplace_back("nonce", blockindex->nNonce);
    result.emplace_back("bits", strprintf("%08x", blockindex->nBits));
    result.emplace_back("difficulty", GetDifficulty(blockindex));
    result.emplace_back("chainwork", blockindex->nChainWork.GetHex());
    result.emplace_back("nTx", blockindex->nTx);
    if (previousblockhash) {
        result.emplace_back("previousblockhash", blockindex->pprev->GetBlockHash().GetHex());
    }
    if (nextblockhash) {
        result.emplace_back("nextblockhash", pnext->GetBlockHash().GetHex());
    }
    return result;
}

UniValue::Object blockToJSON(const Config &config, const CBlock &block, const CBlockIndex *tip, const CBlockIndex *blockindex, bool txDetails) {
    const CBlockIndex *pnext;
    int confirmations = ComputeNextBlockAndDepth(tip, blockindex, pnext);
    bool previousblockhash = blockindex->pprev;
    bool nextblockhash = pnext;
    UniValue::Object result;
    result.reserve(15 + previousblockhash + nextblockhash);
    result.emplace_back("hash", blockindex->GetBlockHash().GetHex());
    result.emplace_back("confirmations", confirmations);
    result.emplace_back("size", ::GetSerializeSize(block, PROTOCOL_VERSION));
    result.emplace_back("height", blockindex->nHeight);
    result.emplace_back("version", block.nVersion);
    result.emplace_back("versionHex", strprintf("%08x", block.nVersion));
    result.emplace_back("merkleroot", block.hashMerkleRoot.GetHex());
    UniValue::Array txs;
    txs.reserve(block.vtx.size());
    for (const auto &tx : block.vtx) {
        if (txDetails) {
            txs.emplace_back(TxToUniv(config, *tx, uint256(), true, RPCSerializationFlags()));
        } else {
            txs.emplace_back(tx->GetId().GetHex());
        }
    }
    result.emplace_back("tx", std::move(txs));
    result.emplace_back("time", block.GetBlockTime());
    result.emplace_back("mediantime", blockindex->GetMedianTimePast());
    result.emplace_back("nonce", block.nNonce);
    result.emplace_back("bits", strprintf("%08x", block.nBits));
    result.emplace_back("difficulty", GetDifficulty(blockindex));
    result.emplace_back("chainwork", blockindex->nChainWork.GetHex());
    result.emplace_back("nTx", blockindex->nTx);
    if (previousblockhash) {
        result.emplace_back("previousblockhash", blockindex->pprev->GetBlockHash().GetHex());
    }
    if (nextblockhash) {
        result.emplace_back("nextblockhash", pnext->GetBlockHash().GetHex());
    }
    return result;
}

static UniValue getblockcount(const Config &config,
                              const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            RPCHelpMan{"getblockcount",
                "\nReturns the number of blocks in the longest blockchain.\n", {}}
                .ToString() +
            "\nResult:\n"
            "n    (numeric) The current block count\n"
            "\nExamples:\n" +
            HelpExampleCli("getblockcount", "") +
            HelpExampleRpc("getblockcount", ""));
    }

    LOCK(cs_main);
    return ::ChainActive().Height();
}

static UniValue getbestblockhash(const Config &config,
                                 const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            RPCHelpMan{"getbestblockhash",
                "\nReturns the hash of the best (tip) block in the longest blockchain.\n", {}}
                .ToString() +
            "\nResult:\n"
            "\"hex\"      (string) the block hash hex-encoded\n"
            "\nExamples:\n" +
            HelpExampleCli("getbestblockhash", "") +
            HelpExampleRpc("getbestblockhash", ""));
    }

    LOCK(cs_main);
    return ::ChainActive().Tip()->GetBlockHash().GetHex();
}

UniValue getfinalizedblockhash(const Config &config,
                               const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "getfinalizedblockhash\n"
            "\nReturns the hash of the currently finalized block\n"
            "\nResult:\n"
            "\"hex\"      (string) the block hash hex-encoded\n");
    }

    LOCK(cs_main);
    const CBlockIndex *blockIndexFinalized = GetFinalizedBlock();
    if (blockIndexFinalized) {
        return blockIndexFinalized->GetBlockHash().GetHex();
    }
    return std::string();
}

void RPCNotifyBlockChange(bool ibd, const CBlockIndex *pindex) {
    if (pindex) {
        std::lock_guard<std::mutex> lock(cs_blockchange);
        latestblock.hash = pindex->GetBlockHash();
        latestblock.height = pindex->nHeight;
    }
    cond_blockchange.notify_all();
}

static UniValue waitfornewblock(const Config &config,
                                const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() > 1) {
        throw std::runtime_error(
            RPCHelpMan{"waitfornewblock",
                "\nWaits for a specific new block and returns useful info about it.\n"
                "\nReturns the current block on timeout or exit.\n",
                {
                    {"timeout", RPCArg::Type::NUM, /* opt */ true, /* default_val */ "", ""},
                }}
                .ToString() +
            "\nArguments:\n"
            "1. timeout (int, optional, default=0) Time in "
            "milliseconds to wait for a response. 0 indicates "
            "no timeout.\n"
            "\nResult:\n"
            "{                           (json object)\n"
            "  \"hash\" : {       (string) The blockhash\n"
            "  \"height\" : {     (int) Block height\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("waitfornewblock", "1000") +
            HelpExampleRpc("waitfornewblock", "1000"));
    }

    int timeout = 0;
    if (!request.params[0].isNull()) {
        timeout = request.params[0].get_int();
    }

    CUpdatedBlock block;
    {
        WAIT_LOCK(cs_blockchange, lock);
        block = latestblock;
        if (timeout) {
            cond_blockchange.wait_for(
                lock, std::chrono::milliseconds(timeout), [&block] {
                    return latestblock.height != block.height ||
                           latestblock.hash != block.hash || !IsRPCRunning();
                });
        } else {
            cond_blockchange.wait(lock, [&block] {
                return latestblock.height != block.height ||
                       latestblock.hash != block.hash || !IsRPCRunning();
            });
        }
        block = latestblock;
    }
    UniValue::Object ret;
    ret.reserve(2);
    ret.emplace_back("hash", block.hash.GetHex());
    ret.emplace_back("height", block.height);
    return ret;
}

static UniValue waitforblock(const Config &config,
                             const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"waitforblock",
                "\nWaits for a specific new block and returns useful info about it.\n"
                "\nReturns the current block on timeout or exit.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "Block hash to wait for."},
                    {"timeout", RPCArg::Type::NUM, /* opt */ true, /* default_val */ "0", "Time in milliseconds to wait for a response. 0 indicates no timeout."},
                }}
                .ToString() +
            "\nResult:\n"
            "{                           (json object)\n"
            "  \"hash\" : {       (string) The blockhash\n"
            "  \"height\" : {     (int) Block height\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("waitforblock", "\"0000000000079f8ef3d2c688c244eb7a4"
                                           "570b24c9ed7b4a8c619eb02596f8862\", "
                                           "1000") +
            HelpExampleRpc("waitforblock", "\"0000000000079f8ef3d2c688c244eb7a4"
                                           "570b24c9ed7b4a8c619eb02596f8862\", "
                                           "1000"));
    }

    int timeout = 0;

    BlockHash hash(ParseHashV(request.params[0], "blockhash"));

    if (!request.params[1].isNull()) {
        timeout = request.params[1].get_int();
    }

    CUpdatedBlock block;
    {
        WAIT_LOCK(cs_blockchange, lock);
        if (timeout) {
            cond_blockchange.wait_for(
                lock, std::chrono::milliseconds(timeout), [&hash] {
                    return latestblock.hash == hash || !IsRPCRunning();
                });
        } else {
            cond_blockchange.wait(lock, [&hash] {
                return latestblock.hash == hash || !IsRPCRunning();
            });
        }
        block = latestblock;
    }

    UniValue::Object ret;
    ret.reserve(2);
    ret.emplace_back("hash", block.hash.GetHex());
    ret.emplace_back("height", block.height);
    return ret;
}

static UniValue waitforblockheight(const Config &config,
                                   const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"waitforblockheight",
                "\nWaits for (at least) block height and returns the height and hash\n"
                "of the current tip.\n"
                "\nReturns the current block on timeout or exit.\n",
                {
                    {"height", RPCArg::Type::NUM, /* opt */ false, /* default_val */ "", "Block height to wait for."},
                    {"timeout", RPCArg::Type::NUM, /* opt */ true, /* default_val */ "0", "Time in milliseconds to wait for a response. 0 indicates no timeout."},
                }}
                .ToString() +
            "\nResult:\n"
            "{                           (json object)\n"
            "  \"hash\" : {       (string) The blockhash\n"
            "  \"height\" : {     (int) Block height\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("waitforblockheight", "\"100\", 1000") +
            HelpExampleRpc("waitforblockheight", "\"100\", 1000"));
    }

    int timeout = 0;

    int height = request.params[0].get_int();

    if (!request.params[1].isNull()) {
        timeout = request.params[1].get_int();
    }

    CUpdatedBlock block;
    {
        WAIT_LOCK(cs_blockchange, lock);
        if (timeout) {
            cond_blockchange.wait_for(
                lock, std::chrono::milliseconds(timeout), [&height] {
                    return latestblock.height >= height || !IsRPCRunning();
                });
        } else {
            cond_blockchange.wait(lock, [&height] {
                return latestblock.height >= height || !IsRPCRunning();
            });
        }
        block = latestblock;
    }
    UniValue::Object ret;
    ret.reserve(2);
    ret.emplace_back("hash", block.hash.GetHex());
    ret.emplace_back("height", block.height);
    return ret;
}

static UniValue
syncwithvalidationinterfacequeue(const Config &config,
                                 const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() > 0) {
        throw std::runtime_error(
            RPCHelpMan{"syncwithvalidationinterfacequeue",
                "\nWaits for the validation interface queue to catch up on everything that was there when we entered this function.\n", {}}
                .ToString() +
            "\nExamples:\n"
            + HelpExampleCli("syncwithvalidationinterfacequeue","")
            + HelpExampleRpc("syncwithvalidationinterfacequeue","")
        );
    }
    SyncWithValidationInterfaceQueue();
    return UniValue();
}

static UniValue getdifficulty(const Config& config,
                              const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            RPCHelpMan{"getdifficulty",
                "\nReturns the proof-of-work difficulty as a multiple of the minimum difficulty.\n", {}}
                .ToString() +
            "\nResult:\n"
            "n.nnn       (numeric) the proof-of-work difficulty as a multiple of the minimum difficulty.\n"
            "\nExamples:\n"
            + HelpExampleCli("getdifficulty", "")
            + HelpExampleRpc("getdifficulty", "")
        );
    }

    LOCK(cs_main);
    return GetDifficulty(::ChainActive().Tip());
}

static std::string EntryDescriptionString() {
    return "    \"size\" : n,             (numeric) transaction size.\n"
           "    \"time\" : n,             (numeric) local time transaction "
           "entered pool in seconds since 1 Jan 1970 GMT\n"
           "    \"fees\" : {\n"
           "        \"base\" : n,         (numeric) transaction fee in " +
           CURRENCY_UNIT +
           "\n"
           "        \"modified\" : n,     (numeric) transaction fee with fee "
           "deltas used for mining priority in " +
           CURRENCY_UNIT +
           "\n"
           "    }\n"
           "    \"depends\" : [           (array) unconfirmed transactions "
           "used as inputs for this transaction\n"
           "        \"transactionid\",    (string) parent transaction id\n"
           "       ... ]\n"
           "    \"spentby\" : [           (array) unconfirmed transactions "
           "spending outputs from this transaction\n"
           "        \"transactionid\",    (string) child transaction id\n"
           "       ... ]\n";
}

static UniValue::Object entryToJSON(const CTxMemPool &pool, const CTxMemPoolEntry &e)
    EXCLUSIVE_LOCKS_REQUIRED(pool.cs) {
    AssertLockHeld(pool.cs);

    UniValue::Object info;
    info.reserve(5);

    UniValue::Object fees;
    fees.reserve(2);
    fees.emplace_back("base", ValueFromAmount(e.GetFee()));
    fees.emplace_back("modified", ValueFromAmount(e.GetModifiedFee()));

    info.emplace_back("fees", std::move(fees));
    info.emplace_back("size", e.GetTxSize());
    info.emplace_back("time", e.GetTime());

    const CTransaction &tx = e.GetTx();

    std::set<std::string> setDepends;
    for (const CTxIn &txin : tx.vin) {
        if (pool.exists(txin.prevout.GetTxId())) {
            setDepends.insert(txin.prevout.GetTxId().ToString());
        }
    }
    UniValue::Array depends;
    depends.reserve(setDepends.size());
    for (const std::string &dep : setDepends) {
        depends.emplace_back(dep);
    }
    info.emplace_back("depends", std::move(depends));

    UniValue::Array spent;
    const CTxMemPool::txiter &it = pool.mapTx.find(tx.GetId());
    const CTxMemPool::setEntries &setChildren = pool.GetMemPoolChildren(it);
    spent.reserve(setChildren.size());
    for (CTxMemPool::txiter childiter : setChildren) {
        spent.emplace_back(childiter->GetTx().GetId().ToString());
    }
    info.emplace_back("spentby", std::move(spent));

    return info;
}

UniValue MempoolToJSON(const CTxMemPool &pool, bool verbose) {
    if (verbose) {
        UniValue::Object ret;
        LOCK(pool.cs);
        ret.reserve(pool.mapTx.size());
        for (const CTxMemPoolEntry &e : pool.mapTx) {
            const uint256 &txid = e.GetTx().GetId();
            ret.emplace_back(txid.ToString(), entryToJSON(pool, e));
        }
        return ret;
    }

    std::vector<uint256> vtxids;
    pool.queryHashes(vtxids);
    UniValue::Array ret;
    ret.reserve(vtxids.size());
    for (const uint256 &txid : vtxids) {
        ret.emplace_back(txid.ToString());
    }
    return ret;
}

static UniValue getrawmempool(const Config &config,
                              const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() > 1) {
        throw std::runtime_error(
            RPCHelpMan{"getrawmempool",
                "\nReturns all transaction ids in memory pool as a json array of string transaction ids.\n"
                "\nHint: use getmempoolentry to fetch a specific transaction from the mempool.\n",
                {
                    {"verbose", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "false", "True for a json object, false for array of transaction ids"},
                }}
                .ToString() +
            "\nResult: (for verbose = false):\n"
            "[                     (json array of string)\n"
            "  \"transactionid\"     (string) The transaction id\n"
            "  ,...\n"
            "]\n"
            "\nResult: (for verbose = true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n" +
            EntryDescriptionString() +
            "  }, ...\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getrawmempool", "true") +
            HelpExampleRpc("getrawmempool", "true"));
    }

    bool fVerbose = false;
    if (!request.params[0].isNull()) {
        fVerbose = request.params[0].get_bool();
    }

    return MempoolToJSON(::g_mempool, fVerbose);
}

static UniValue getmempoolancestors(const Config &config,
                                    const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"getmempoolancestors",
                "\nIf txid is in the mempool, returns all in-mempool ancestors.\n",
                {
                    {"txid", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The transaction id (must be in mempool)"},
                    {"verbose", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "false", "True for a json object, false for array of transaction ids"},
                }}
                .ToString() +
            "\nResult (for verbose = false):\n"
            "[                       (json array of strings)\n"
            "  \"transactionid\"           (string) The transaction id of an "
            "in-mempool ancestor transaction\n"
            "  ,...\n"
            "]\n"
            "\nResult (for verbose = true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n" +
            EntryDescriptionString() +
            "  }, ...\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getmempoolancestors", "\"mytxid\"") +
            HelpExampleRpc("getmempoolancestors", "\"mytxid\""));
    }

    bool fVerbose = false;
    if (!request.params[1].isNull()) {
        fVerbose = request.params[1].get_bool();
    }

    TxId txid(ParseHashV(request.params[0], "parameter 1"));

    LOCK(g_mempool.cs);

    CTxMemPool::txiter it = g_mempool.mapTx.find(txid);
    if (it == g_mempool.mapTx.end()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           "Transaction not in mempool");
    }

    CTxMemPool::setEntries setAncestors;
    g_mempool.CalculateMemPoolAncestors(*it, setAncestors, false);

    if (!fVerbose) {
        UniValue::Array ret;
        ret.reserve(setAncestors.size());
        for (CTxMemPool::txiter ancestorIt : setAncestors) {
            ret.emplace_back(ancestorIt->GetTx().GetId().ToString());
        }
        return ret;
    }

    UniValue::Object ret;
    ret.reserve(setAncestors.size());
    for (CTxMemPool::txiter ancestorIt : setAncestors) {
        const CTxMemPoolEntry &e = *ancestorIt;
        const TxId &_txid = e.GetTx().GetId();
        ret.emplace_back(_txid.ToString(), entryToJSON(::g_mempool, e));
    }
    return ret;
}

static UniValue getmempooldescendants(const Config &config,
                                      const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"getmempooldescendants",
                "\nIf txid is in the mempool, returns all in-mempool descendants.\n",
                {
                    {"txid", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The transaction id (must be in mempool)"},
                    {"verbose", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "false", "True for a json object, false for array of transaction ids"},
                }}
                .ToString() +
            "\nResult (for verbose = false):\n"
            "[                       (json array of strings)\n"
            "  \"transactionid\"           (string) The transaction id of an "
            "in-mempool descendant transaction\n"
            "  ,...\n"
            "]\n"
            "\nResult (for verbose = true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n" +
            EntryDescriptionString() +
            "  }, ...\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getmempooldescendants", "\"mytxid\"") +
            HelpExampleRpc("getmempooldescendants", "\"mytxid\""));
    }

    bool fVerbose = false;
    if (!request.params[1].isNull()) {
        fVerbose = request.params[1].get_bool();
    }

    TxId txid(ParseHashV(request.params[0], "parameter 1"));

    LOCK(g_mempool.cs);

    CTxMemPool::txiter it = g_mempool.mapTx.find(txid);
    if (it == g_mempool.mapTx.end()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           "Transaction not in mempool");
    }

    CTxMemPool::setEntries setDescendants;
    g_mempool.CalculateDescendants(it, setDescendants);
    // CTxMemPool::CalculateDescendants will include the given tx
    setDescendants.erase(it);

    if (!fVerbose) {
        UniValue::Array ret;
        ret.reserve(setDescendants.size());
        for (CTxMemPool::txiter descendantIt : setDescendants) {
            ret.emplace_back(descendantIt->GetTx().GetId().ToString());
        }
        return ret;
    }

    UniValue::Object ret;
    ret.reserve(setDescendants.size());
    for (CTxMemPool::txiter descendantIt : setDescendants) {
        const CTxMemPoolEntry &e = *descendantIt;
        const TxId &_txid = e.GetTx().GetId();
        ret.emplace_back(_txid.ToString(), entryToJSON(::g_mempool, e));
    }
    return ret;
}

static UniValue getmempoolentry(const Config &config,
                                const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"getmempoolentry",
                "\nReturns mempool data for given transaction\n",
                {
                    {"txid", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The transaction id (must be in mempool)"},
                }}
                .ToString() +
            "\nResult:\n"
            "{                           (json object)\n" +
            EntryDescriptionString() +
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getmempoolentry", "\"mytxid\"") +
            HelpExampleRpc("getmempoolentry", "\"mytxid\""));
    }

    TxId txid(ParseHashV(request.params[0], "parameter 1"));

    LOCK(g_mempool.cs);

    CTxMemPool::txiter it = g_mempool.mapTx.find(txid);
    if (it == g_mempool.mapTx.end()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           "Transaction not in mempool");
    }

    const CTxMemPoolEntry &e = *it;
    return entryToJSON(::g_mempool, e);
}

static UniValue getblockhash(const Config &config,
                             const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"getblockhash",
                "\nReturns hash of block in best-block-chain at height provided.\n",
                {
                    {"height", RPCArg::Type::NUM, /* opt */ false, /* default_val */ "", "The height index"},
                }}
                .ToString() +
            "\nResult:\n"
            "\"hash\"         (string) The block hash\n"
            "\nExamples:\n" +
            HelpExampleCli("getblockhash", "1000") +
            HelpExampleRpc("getblockhash", "1000"));
    }

    LOCK(cs_main);

    int nHeight = request.params[0].get_int();
    if (nHeight < 0 || nHeight > ::ChainActive().Height()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height out of range");
    }

    CBlockIndex *pblockindex = ::ChainActive()[nHeight];
    return pblockindex->GetBlockHash().GetHex();
}

static UniValue getblockheader(const Config &config,
                               const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"getblockheader",
                "\nIf verbose is false, returns a string that is serialized, hex-encoded data for blockheader 'hash'.\n"
                "If verbose is true, returns an Object with information about blockheader <hash>.\n",
                {
                    {"hash_or_height", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "The block hash or block height"},
                    {"verbose", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "true", "true for a json object, false for the hex-encoded data"},
                }}
                .ToString() +
            "\nResult (for verbose = true):\n"
            "{\n"
            "  \"hash\" : \"hash\",     (string) the block hash (same as "
            "provided)\n"
            "  \"confirmations\" : n,   (numeric) The number of confirmations, "
            "or -1 if the block is not on the main chain\n"
            "  \"height\" : n,          (numeric) The block height or index\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"versionHex\" : \"00000000\", (string) The block version "
            "formatted in hexadecimal\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds "
            "since epoch (Jan 1 1970 GMT)\n"
            "  \"mediantime\" : ttt,    (numeric) The median block time in "
            "seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\", (string) The bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) The difficulty\n"
            "  \"chainwork\" : \"0000...1f3\"     (string) Expected number of "
            "hashes required to produce the current chain (in hex)\n"
            "  \"nTx\" : n,             (numeric) The number of transactions "
            "in the block.\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the "
            "previous block\n"
            "  \"nextblockhash\" : \"hash\",      (string) The hash of the "
            "next block\n"
            "}\n"
            "\nResult (for verbose=false):\n"
            "\"data\"             (string) A string that is serialized, "
            "hex-encoded data for block 'hash'.\n"
            "\nExamples:\n" +
            HelpExampleCli("getblockheader", "1000") +
            HelpExampleRpc("getblockheader", "1000") +
            HelpExampleCli("getblockheader", "'\"00000000c937983704a73af28acdec3"
                                             "7b049d214adbda81d7e2a3dd146f6ed09"
                                             "\"'") +
            HelpExampleRpc("getblockheader", "\"00000000c937983704a73af28acdec3"
                                             "7b049d214adbda81d7e2a3dd146f6ed09"
                                             "\""));
    }

    const CBlockIndex *pindex{};
    const CBlockIndex *tip{};

    {
        LOCK(cs_main);
        if (request.params[0].isNum()) {
            const int height = request.params[0].get_int();
            if (height < 0) {
                throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    strprintf("Target block height %d is negative", height));
            }
            tip = ::ChainActive().Tip();
            if (height > tip->nHeight) {
                throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    strprintf("Target block height %d after current tip %d", height,
                              tip->nHeight));
            }
            pindex = ::ChainActive()[height];
        } else {
            const BlockHash hash(ParseHashV(request.params[0], "hash_or_height"));
            pindex = LookupBlockIndex(hash);
            if (!pindex) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
            }
            tip = ::ChainActive().Tip();
        }
    }

    assert(pindex != nullptr);

    bool fVerbose = true;
    if (!request.params[1].isNull()) {
        fVerbose = request.params[1].get_bool();
    }

    if (!fVerbose) {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << pindex->GetBlockHeader();
        return HexStr(ssBlock);
    }

    return blockheaderToJSON(tip, pindex);
}

/// Requires cs_main; called by getblock() and getblockstats()
static void ThrowIfPrunedBlock(const CBlockIndex *pblockindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
    if (IsBlockPruned(pblockindex)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Block not available (pruned data)");
    }
}

/// Lock-free -- will throw if block not found or was pruned, etc. Guaranteed to return a valid block or fail.
static CBlock ReadBlockChecked(const Config &config, const CBlockIndex *pblockindex) {
    CBlock block;
    auto doRead = [&] {
        if (!ReadBlockFromDisk(block, pblockindex,
                               config.GetChainParams().GetConsensus())) {
            // Block not found on disk. This could be because we have the block
            // header in our index but don't have the block (for example if a
            // non-whitelisted node sends us an unrequested long chain of valid
            // blocks, we add the headers to our index, but don't accept the block).
            throw JSONRPCError(RPC_MISC_ERROR, "Block not found on disk");
        }
    };
    if (fPruneMode) {
        // Note: in pruned mode we must take cs_main here because it's possible for FlushStateToDisk()
        // in validation.cpp to also attempt to remove this file while we have it open.  This is not
        // normally a problem except for on Windows, where FlushStateToDisk() would fail to remove the
        // block file we have open here, in which case on Windows the node would AbortNode().  Hence
        // the need for this locking in the fPrunedMode case only.
        LOCK(cs_main);
        doRead();
    } else {
        // Non-pruned mode, we can benefit from not having to grab cs_main here since blocks never
        // go away -- this increases parallelism in the case of non-pruning nodes.
        doRead();
    }

    return block;
}

static UniValue getblock(const Config &config, const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"getblock",
                "\nIf verbosity is 0 or false, returns a string that is serialized, hex-encoded data for block 'hash'.\n"
                "If verbosity is 1 or true, returns an Object with information about block <hash>.\n"
                "If verbosity is 2, returns an Object with information about block <hash> and information about each transaction.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The block hash"},
                    {"verbosity", RPCArg::Type::NUM, /* opt */ true, /* default_val */ "1", "0 for hex-encoded data, 1 for a json object, and 2 for json object with transaction data"},
                }}
                .ToString() +
            "\nResult (for verbosity = 0):\n"
            "\"data\"                   (string) A string that is serialized, "
            "hex-encoded data for block 'hash'.\n"
            "\nResult (for verbosity = 1):\n"
            "{\n"
            "  \"hash\" : \"hash\",       (string) The block hash (same as "
            "provided)\n"
            "  \"confirmations\" : n,   (numeric) The number of confirmations, "
            "or -1 if the block is not on the main chain\n"
            "  \"size\" : n,            (numeric) The block size\n"
            "  \"height\" : n,          (numeric) The block height or index\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"versionHex\" : \"00000000\", (string) The block version "
            "formatted in hexadecimal\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"tx\" : [               (array of string) The transaction ids\n"
            "     \"transactionid\"     (string) The transaction id\n"
            "     ,...\n"
            "  ],\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds "
            "since epoch (Jan 1 1970 GMT)\n"
            "  \"mediantime\" : ttt,    (numeric) The median block time in "
            "seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\",   (string) The bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) The difficulty\n"
            "  \"chainwork\" : \"xxxx\",  (string) Expected number of hashes "
            "required to produce the chain up to this block (in hex)\n"
            "  \"nTx\" : n,             (numeric) The number of transactions "
            "in the block.\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the "
            "previous block\n"
            "  \"nextblockhash\" : \"hash\"       (string) The hash of the "
            "next block\n"
            "}\n"
            "\nResult (for verbosity = 2):\n"
            "{\n"
            "  ...,                   Same output as verbosity = 1\n"
            "  \"tx\" : [               (array of Objects) The transactions in "
            "the format of the getrawtransaction RPC; different from verbosity "
            "= 1 \"tx\" result\n"
            "    ...\n"
            "  ],\n"
            "  ...                    Same output as verbosity = 1\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getblock", "\"00000000c937983704a73af28acdec37b049d"
                                       "214adbda81d7e2a3dd146f6ed09\"") +
            HelpExampleRpc("getblock", "\"00000000c937983704a73af28acdec37b049d"
                                       "214adbda81d7e2a3dd146f6ed09\""));
    }

    BlockHash hash(ParseHashV(request.params[0], "blockhash"));

    int verbosity = 1;
    if (!request.params[1].isNull()) {
        if (request.params[1].isNum()) {
            verbosity = request.params[1].get_int();
        } else {
            verbosity = request.params[1].get_bool() ? 1 : 0;
        }
    }

    const CBlockIndex *pblockindex{};
    {
        LOCK(cs_main);
        pblockindex = LookupBlockIndex(hash);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
        ThrowIfPrunedBlock(pblockindex);
    }

    const CBlock block = ReadBlockChecked(config, pblockindex);

    if (verbosity <= 0) {
        CDataStream ssBlock(SER_NETWORK,
                            PROTOCOL_VERSION | RPCSerializationFlags());
        ssBlock << block;
        std::string strHex = HexStr(ssBlock);
        return strHex;
    }

    return blockToJSON(config, block, ::ChainActive().Tip(), pblockindex, verbosity >= 2);
}

struct CCoinsStats {
    int nHeight;
    BlockHash hashBlock;
    uint64_t nTransactions;
    uint64_t nTransactionOutputs;
    uint64_t nBogoSize;
    uint256 hashSerialized;
    uint64_t nDiskSize;
    Amount nTotalAmount;

    CCoinsStats()
        : nHeight(0), nTransactions(0), nTransactionOutputs(0), nBogoSize(0),
          nDiskSize(0), nTotalAmount() {}
};

static void ApplyStats(CCoinsStats &stats, CHashWriter &ss, const uint256 &hash,
                       const std::map<uint32_t, Coin> &outputs) {
    assert(!outputs.empty());
    ss << hash;
    ss << VARINT(outputs.begin()->second.GetHeight() * 2 +
                 outputs.begin()->second.IsCoinBase());
    stats.nTransactions++;
    for (const auto &output : outputs) {
        ss << VARINT(output.first + 1);
        ss << output.second.GetTxOut().scriptPubKey;
        ss << VARINT_MODE(output.second.GetTxOut().nValue / SATOSHI,
                          VarIntMode::NONNEGATIVE_SIGNED);
        stats.nTransactionOutputs++;
        stats.nTotalAmount += output.second.GetTxOut().nValue;
        stats.nBogoSize +=
            32 /* txid */ + 4 /* vout index */ + 4 /* height + coinbase */ +
            8 /* amount */ + 2 /* scriptPubKey len */ +
            output.second.GetTxOut().scriptPubKey.size() /* scriptPubKey */;
    }
    ss << VARINT(0u);
}

//! Calculate statistics about the unspent transaction output set
static bool GetUTXOStats(CCoinsView *view, CCoinsStats &stats) {
    std::unique_ptr<CCoinsViewCursor> pcursor(view->Cursor());
    assert(pcursor);

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    stats.hashBlock = pcursor->GetBestBlock();
    {
        LOCK(cs_main);
        stats.nHeight = LookupBlockIndex(stats.hashBlock)->nHeight;
    }
    ss << stats.hashBlock;
    uint256 prevkey;
    std::map<uint32_t, Coin> outputs;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        COutPoint key;
        Coin coin;
        if (pcursor->GetKey(key) && pcursor->GetValue(coin)) {
            if (!outputs.empty() && key.GetTxId() != prevkey) {
                ApplyStats(stats, ss, prevkey, outputs);
                outputs.clear();
            }
            prevkey = key.GetTxId();
            outputs[key.GetN()] = std::move(coin);
        } else {
            return error("%s: unable to read value", __func__);
        }
        pcursor->Next();
    }
    if (!outputs.empty()) {
        ApplyStats(stats, ss, prevkey, outputs);
    }
    stats.hashSerialized = ss.GetHash();
    stats.nDiskSize = view->EstimateSize();
    return true;
}

static UniValue pruneblockchain(const Config &config,
                                const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"pruneblockchain", "",
                {
                    {"height", RPCArg::Type::NUM, /* opt */ false, /* default_val */ "", "The block height to prune up to. May be set to a discrete height, or a unix timestamp\n"
            "                  to prune blocks whose block time is at least 2 hours older than the provided timestamp."},
                }}
                .ToString() +
            "\nResult:\n"
            "n    (numeric) Height of the last block pruned.\n"
            "\nExamples:\n" +
            HelpExampleCli("pruneblockchain", "1000") +
            HelpExampleRpc("pruneblockchain", "1000"));
    }

    if (!fPruneMode) {
        throw JSONRPCError(
            RPC_MISC_ERROR,
            "Cannot prune blocks because node is not in prune mode.");
    }

    LOCK(cs_main);

    int heightParam = request.params[0].get_int();
    if (heightParam < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative block height.");
    }

    // Height value more than a billion is too high to be a block height, and
    // too low to be a block time (corresponds to timestamp from Sep 2001).
    if (heightParam > 1000000000) {
        // Add a 2 hour buffer to include blocks which might have had old
        // timestamps
        CBlockIndex *pindex =
            ::ChainActive().FindEarliestAtLeast(heightParam - TIMESTAMP_WINDOW);
        if (!pindex) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "Could not find block with at least the specified timestamp.");
        }
        heightParam = pindex->nHeight;
    }

    unsigned int height = (unsigned int)heightParam;
    unsigned int chainHeight = (unsigned int)::ChainActive().Height();
    if (chainHeight < config.GetChainParams().PruneAfterHeight()) {
        throw JSONRPCError(RPC_MISC_ERROR,
                           "Blockchain is too short for pruning.");
    } else if (height > chainHeight) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            "Blockchain is shorter than the attempted prune height.");
    } else if (height > chainHeight - MIN_BLOCKS_TO_KEEP) {
        LogPrint(BCLog::RPC, "Attempt to prune blocks close to the tip. "
                             "Retaining the minimum number of blocks.\n");
        height = chainHeight - MIN_BLOCKS_TO_KEEP;
    }

    PruneBlockFilesManual(height);
    return height;
}

static UniValue gettxoutsetinfo(const Config &config,
                                const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            RPCHelpMan{"gettxoutsetinfo",
                "\nReturns statistics about the unspent transaction output set.\n"
                "Note this call may take some time.\n",
                {}}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"height\":n,     (numeric) The current block height (index)\n"
            "  \"bestblock\": \"hex\",   (string) the best block hash hex\n"
            "  \"transactions\": n,      (numeric) The number of transactions\n"
            "  \"txouts\": n,            (numeric) The number of output "
            "transactions\n"
            "  \"bogosize\": n,          (numeric) A database-independent "
            "metric for UTXO set size\n"
            "  \"hash_serialized\": \"hash\",   (string) The serialized hash\n"
            "  \"disk_size\": n,         (numeric) The estimated size of the "
            "chainstate on disk\n"
            "  \"total_amount\": x.xxx          (numeric) The total amount\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("gettxoutsetinfo", "") +
            HelpExampleRpc("gettxoutsetinfo", ""));
    }

    CCoinsStats stats;
    FlushStateToDisk();
    if (!GetUTXOStats(pcoinsdbview.get(), stats)) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Unable to read UTXO set");
    }

    UniValue::Object ret;
    ret.reserve(8);
    ret.emplace_back("height", stats.nHeight);
    ret.emplace_back("bestblock", stats.hashBlock.GetHex());
    ret.emplace_back("transactions", stats.nTransactions);
    ret.emplace_back("txouts", stats.nTransactionOutputs);
    ret.emplace_back("bogosize", stats.nBogoSize);
    ret.emplace_back("hash_serialized", stats.hashSerialized.GetHex());
    ret.emplace_back("disk_size", stats.nDiskSize);
    ret.emplace_back("total_amount", ValueFromAmount(stats.nTotalAmount));
    return ret;
}

UniValue gettxout(const Config &config, const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 2 ||
        request.params.size() > 3) {
        throw std::runtime_error(
            RPCHelpMan{"gettxout",
                "\nReturns details about an unspent transaction output.\n",
                {
                    {"txid", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "The transaction id"},
                    {"n", RPCArg::Type::NUM, /* opt */ false, /* default_val */ "", "vout number"},
                    {"include_mempool", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "true", "Whether to include the mempool. Note that an unspent output that is spent in the mempool won't appear."},
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"bestblock\" : \"hash\",    (string) the block hash\n"
            "  \"confirmations\" : n,       (numeric) The number of "
            "confirmations\n"
            "  \"value\" : x.xxx,           (numeric) The transaction value "
            "in " +
            CURRENCY_UNIT +
            "\n"
            "  \"scriptPubKey\" : {         (json object)\n"
            "     \"asm\" : \"code\",       (string)\n"
            "     \"hex\" : \"hex\",        (string)\n"
            "     \"reqSigs\" : n,          (numeric) Number of required "
            "signatures\n"
            "     \"type\" : \"pubkeyhash\", (string) The type, eg pubkeyhash\n"
            "     \"addresses\" : [          (array of string) array of Bitcoin Cash addresses\n"
            "        \"address\"     (string) Bitcoin Cash address\n"
            "        ,...\n"
            "     ]\n"
            "  },\n"
            "  \"coinbase\" : true|false   (boolean) Coinbase or not\n"
            "}\n"

            "\nExamples:\n"
            "\nGet unspent transactions\n" +
            HelpExampleCli("listunspent", "") + "\nView the details\n" +
            HelpExampleCli("gettxout", "\"txid\" 1") +
            "\nAs a JSON-RPC call\n" +
            HelpExampleRpc("gettxout", "\"txid\", 1"));
    }

    LOCK(cs_main);

    TxId txid(ParseHashV(request.params[0], "txid"));
    int n = request.params[1].get_int();
    COutPoint out(txid, n);
    bool fMempool = true;
    if (!request.params[2].isNull()) {
        fMempool = request.params[2].get_bool();
    }

    Coin coin;
    if (fMempool) {
        LOCK(g_mempool.cs);
        CCoinsViewMemPool view(pcoinsTip.get(), g_mempool);
        if (!view.GetCoin(out, coin) || g_mempool.isSpent(out)) {
            return UniValue();
        }
    } else {
        if (!pcoinsTip->GetCoin(out, coin)) {
            return UniValue();
        }
    }

    const CBlockIndex *pindex = LookupBlockIndex(pcoinsTip->GetBestBlock());
    UniValue::Object ret;
    ret.reserve(5);
    ret.emplace_back("bestblock", pindex->GetBlockHash().GetHex());
    ret.emplace_back("confirmations", coin.GetHeight() == MEMPOOL_HEIGHT ? 0 : pindex->nHeight - coin.GetHeight() + 1);
    ret.emplace_back("value", ValueFromAmount(coin.GetTxOut().nValue));
    ret.emplace_back("scriptPubKey", ScriptPubKeyToUniv(config, coin.GetTxOut().scriptPubKey, true));
    ret.emplace_back("coinbase", coin.IsCoinBase());

    return ret;
}

static UniValue verifychain(const Config &config,
                            const JSONRPCRequest &request) {
    int nCheckLevel = gArgs.GetArg("-checklevel", DEFAULT_CHECKLEVEL);
    int nCheckDepth = gArgs.GetArg("-checkblocks", DEFAULT_CHECKBLOCKS);
    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"verifychain",
                "\nVerifies blockchain database.\n",
                {
                    {"checklevel", RPCArg::Type::NUM, /* opt */ true, /* default_val */ strprintf("%d, range=0-4", nCheckLevel), "How thorough the block verification is."},
                    {"nblocks", RPCArg::Type::NUM, /* opt */ true, /* default_val */ strprintf("%d, 0=all", nCheckDepth), "The number of blocks to check."},
                }}
                .ToString() +
            "\nResult:\n"
            "true|false       (boolean) Verified or not\n"
            "\nExamples:\n" +
            HelpExampleCli("verifychain", "") +
            HelpExampleRpc("verifychain", ""));
    }

    LOCK(cs_main);

    if (!request.params[0].isNull()) {
        nCheckLevel = request.params[0].get_int();
    }
    if (!request.params[1].isNull()) {
        nCheckDepth = request.params[1].get_int();
    }

    return CVerifyDB().VerifyDB(config, pcoinsTip.get(), nCheckLevel,
                                nCheckDepth);
}

UniValue getblockchaininfo(const Config &config,
                           const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            RPCHelpMan{"getblockchaininfo",
                "Returns an object containing various state info regarding blockchain processing.\n", {}}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"chain\": \"xxxx\",              (string) current network name "
            "as defined in BIP70 (main, test, regtest)\n"
            "  \"blocks\": xxxxxx,             (numeric) the current number of "
            "blocks processed in the server\n"
            "  \"headers\": xxxxxx,            (numeric) the current number of "
            "headers we have validated\n"
            "  \"bestblockhash\": \"...\",       (string) the hash of the "
            "currently best block\n"
            "  \"difficulty\": xxxxxx,         (numeric) the current "
            "difficulty\n"
            "  \"mediantime\": xxxxxx,         (numeric) median time for the "
            "current best block\n"
            "  \"verificationprogress\": xxxx, (numeric) estimate of "
            "verification progress [0..1]\n"
            "  \"initialblockdownload\": xxxx, (bool) (debug information) "
            "estimate of whether this node is in Initial Block Download mode.\n"
            "  \"chainwork\": \"xxxx\"           (string) total amount of work "
            "in active chain, in hexadecimal\n"
            "  \"size_on_disk\": xxxxxx,       (numeric) the estimated size of "
            "the block and undo files on disk\n"
            "  \"pruned\": xx,                 (boolean) if the blocks are "
            "subject to pruning\n"
            "  \"pruneheight\": xxxxxx,        (numeric) lowest-height "
            "complete block stored (only present if pruning is enabled)\n"
            "  \"automatic_pruning\": xx,      (boolean) whether automatic "
            "pruning is enabled (only present if pruning is enabled)\n"
            "  \"prune_target_size\": xxxxxx,  (numeric) the target size "
            "used by pruning (only present if automatic pruning is enabled)\n"
            "  \"warnings\" : \"...\",           (string) any network and "
            "blockchain warnings.\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getblockchaininfo", "") +
            HelpExampleRpc("getblockchaininfo", ""));
    }

    LOCK(cs_main);

    const CBlockIndex *tip = ::ChainActive().Tip();
    bool automatic_pruning = fPruneMode && gArgs.GetArg("-prune", 0) != 1;
    UniValue::Object obj;
    obj.reserve(fPruneMode ? automatic_pruning ? 15 : 14 : 12);

    obj.emplace_back("chain", config.GetChainParams().NetworkIDString());
    obj.emplace_back("blocks", ::ChainActive().Height());
    obj.emplace_back("headers", pindexBestHeader ? pindexBestHeader->nHeight : -1);
    obj.emplace_back("bestblockhash", tip->GetBlockHash().GetHex());
    obj.emplace_back("difficulty", GetDifficulty(tip));
    obj.emplace_back("mediantime", tip->GetMedianTimePast());
    obj.emplace_back("verificationprogress", GuessVerificationProgress(Params().TxData(), tip));
    obj.emplace_back("initialblockdownload", IsInitialBlockDownload());
    obj.emplace_back("chainwork", tip->nChainWork.GetHex());
    obj.emplace_back("size_on_disk", CalculateCurrentUsage());
    obj.emplace_back("pruned", fPruneMode);

    if (fPruneMode) {
        const CBlockIndex *block = tip;
        assert(block);
        while (block->pprev && block->pprev->nStatus.hasData()) {
            block = block->pprev;
        }

        obj.emplace_back("pruneheight", block->nHeight);

        obj.emplace_back("automatic_pruning", automatic_pruning);
        if (automatic_pruning) {
            obj.emplace_back("prune_target_size", nPruneTarget);
        }
    }

    obj.emplace_back("warnings", GetWarnings("statusbar"));
    return obj;
}

/** Comparison function for sorting the getchaintips heads.  */
struct CompareBlocksByHeight {
    bool operator()(const CBlockIndex *a, const CBlockIndex *b) const {
        // Make sure that unequal blocks with the same height do not compare
        // equal. Use the pointers themselves to make a distinction.
        if (a->nHeight != b->nHeight) {
            return (a->nHeight > b->nHeight);
        }

        return a < b;
    }
};

static UniValue getchaintips(const Config &config,
                             const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            RPCHelpMan{"getchaintips",
                "Return information about all known tips in the block tree,"
                " including the main chain as well as orphaned branches.\n",
                {}}
                .ToString() +
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"height\": xxxx,         (numeric) height of the chain tip\n"
            "    \"hash\": \"xxxx\",         (string) block hash of the tip\n"
            "    \"branchlen\": 0          (numeric) zero for main chain\n"
            "    \"status\": \"active\"      (string) \"active\" for the main "
            "chain\n"
            "  },\n"
            "  {\n"
            "    \"height\": xxxx,\n"
            "    \"hash\": \"xxxx\",\n"
            "    \"branchlen\": 1          (numeric) length of branch "
            "connecting the tip to the main chain\n"
            "    \"status\": \"xxxx\"        (string) status of the chain "
            "(active, valid-fork, valid-headers, headers-only, invalid)\n"
            "  }\n"
            "]\n"
            "Possible values for status:\n"
            "1.  \"invalid\"               This branch contains at least one "
            "invalid block\n"
            "2.  \"parked\"                This branch contains at least one "
            "parked block\n"
            "3.  \"headers-only\"          Not all blocks for this branch are "
            "available, but the headers are valid\n"
            "4.  \"valid-headers\"         All blocks are available for this "
            "branch, but they were never fully validated\n"
            "5.  \"valid-fork\"            This branch is not part of the "
            "active chain, but is fully validated\n"
            "6.  \"active\"                This is the tip of the active main "
            "chain, which is certainly valid\n"
            "\nExamples:\n" +
            HelpExampleCli("getchaintips", "") +
            HelpExampleRpc("getchaintips", ""));
    }

    LOCK(cs_main);

    /**
     * Idea:  the set of chain tips is ::ChainActive().tip, plus orphan blocks
     * which do not have another orphan building off of them. Algorithm:
     *  - Make one pass through mapBlockIndex, picking out the orphan blocks,
     * and also storing a set of the orphan block's pprev pointers.
     *  - Iterate through the orphan blocks. If the block isn't pointed to by
     * another orphan, it is a chain tip.
     *  - add ::ChainActive().Tip()
     */
    std::set<const CBlockIndex *, CompareBlocksByHeight> setTips;
    std::set<const CBlockIndex *> setOrphans;
    std::set<const CBlockIndex *> setPrevs;

    for (const std::pair<const BlockHash, CBlockIndex *> &item :
         mapBlockIndex) {
        if (!::ChainActive().Contains(item.second)) {
            setOrphans.insert(item.second);
            setPrevs.insert(item.second->pprev);
        }
    }

    for (std::set<const CBlockIndex *>::iterator it = setOrphans.begin();
         it != setOrphans.end(); ++it) {
        if (setPrevs.erase(*it) == 0) {
            setTips.insert(*it);
        }
    }

    // Always report the currently active tip.
    setTips.insert(::ChainActive().Tip());

    /* Construct the output array.  */
    UniValue::Array res;
    res.reserve(setTips.size());
    for (const CBlockIndex *block : setTips) {
        UniValue::Object obj;
        obj.reserve(4);
        obj.emplace_back("height", block->nHeight);
        obj.emplace_back("hash", block->phashBlock->GetHex());
        obj.emplace_back("branchlen", block->nHeight - ::ChainActive().FindFork(block)->nHeight);

        const char *status;
        if (::ChainActive().Contains(block)) {
            // This block is part of the currently active chain.
            status = "active";
        } else if (block->nStatus.isInvalid()) {
            // This block or one of its ancestors is invalid.
            status = "invalid";
        } else if (block->nStatus.isOnParkedChain()) {
            // This block or one of its ancestors is parked.
            status = "parked";
        } else if (!block->HaveTxsDownloaded()) {
            // This block cannot be connected because full block data for it or
            // one of its parents is missing.
            status = "headers-only";
        } else if (block->IsValid(BlockValidity::SCRIPTS)) {
            // This block is fully validated, but no longer part of the active
            // chain. It was probably the active block once, but was
            // reorganized.
            status = "valid-fork";
        } else if (block->IsValid(BlockValidity::TREE)) {
            // The headers for this block are valid, but it has not been
            // validated. It was probably never part of the most-work chain.
            status = "valid-headers";
        } else {
            // No clue.
            status = "unknown";
        }
        obj.emplace_back("status", status);

        res.emplace_back(std::move(obj));
    }

    return res;
}

UniValue::Object MempoolInfoToJSON(const Config &config, const CTxMemPool &pool) {
    UniValue::Object ret;
    ret.reserve(7);
    ret.emplace_back("loaded", pool.IsLoaded());
    ret.emplace_back("size", pool.size());
    ret.emplace_back("bytes", pool.GetTotalTxSize());
    ret.emplace_back("usage", pool.DynamicMemoryUsage());
    auto maxmempool = config.GetMaxMemPoolSize();
    ret.emplace_back("maxmempool", maxmempool);
    ret.emplace_back("mempoolminfee", ValueFromAmount(std::max(pool.GetMinFee(maxmempool), ::minRelayTxFee).GetFeePerK()));
    ret.emplace_back("minrelaytxfee", ValueFromAmount(::minRelayTxFee.GetFeePerK()));
    return ret;
}

static UniValue getmempoolinfo(const Config &config,
                               const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            RPCHelpMan{"getmempoolinfo",
                "\nReturns details on the active state of the TX memory pool.\n", {}}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"loaded\": true|false         (boolean) True if the mempool is "
            "fully loaded\n"
            "  \"size\": xxxxx,               (numeric) Current tx count\n"
            "  \"bytes\": xxxxx,              (numeric) Transaction size.\n"
            "  \"usage\": xxxxx,              (numeric) Total memory usage for "
            "the mempool\n"
            "  \"maxmempool\": xxxxx,         (numeric) Maximum memory usage "
            "for the mempool\n"
            "  \"mempoolminfee\": xxxxx       (numeric) Minimum fee rate in " +
            CURRENCY_UNIT +
            "/kB for tx to be accepted. Is the maximum of minrelaytxfee and "
            "minimum mempool fee\n"
            "  \"minrelaytxfee\": xxxxx       (numeric) Current minimum relay "
            "fee for transactions\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getmempoolinfo", "") +
            HelpExampleRpc("getmempoolinfo", ""));
    }

    return MempoolInfoToJSON(config, ::g_mempool);
}

static UniValue preciousblock(const Config &config,
                              const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"preciousblock",
                "\nTreats a block as if it were received before others with the same work.\n"
                "\nA later preciousblock call can override the effect of an earlier one.\n"
                "\nThe effects of preciousblock are not retained across restarts.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "the hash of the block to mark as precious"},
                }}
                .ToString() +
            "\nResult:\n"
            "\nExamples:\n" +
            HelpExampleCli("preciousblock", "\"blockhash\"") +
            HelpExampleRpc("preciousblock", "\"blockhash\""));
    }

    BlockHash hash(ParseHashV(request.params[0], "blockhash"));
    CBlockIndex *pblockindex;

    {
        LOCK(cs_main);
        pblockindex = LookupBlockIndex(hash);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
    }

    CValidationState state;
    PreciousBlock(config, state, pblockindex);

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return UniValue();
}

UniValue finalizeblock(const Config &config, const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "finalizeblock \"blockhash\"\n"

            "\nTreats a block as final. It cannot be reorged. Any chain\n"
            "that does not contain this block is invalid. Used on a less\n"
            "work chain, it can effectively PUTS YOU OUT OF CONSENSUS.\n"
            "USE WITH CAUTION!\n"
            "\nResult:\n"
            "\nExamples:\n" +
            HelpExampleCli("finalizeblock", "\"blockhash\"") +
            HelpExampleRpc("finalizeblock", "\"blockhash\""));
    }

    const BlockHash hash(ParseHashV(request.params[0], "blockhash"));
    CValidationState state;

    {
        LOCK(cs_main);
        CBlockIndex *pblockindex = LookupBlockIndex(hash);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }

        FinalizeBlockAndInvalidate(config, state, pblockindex);
    }

    if (state.IsValid()) {
        ActivateBestChain(config, state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, FormatStateMessage(state));
    }

    return UniValue();
}

static UniValue invalidateblock(const Config &config,
                                const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"invalidateblock",
                "\nPermanently marks a block as invalid, as if it violated a consensus rule.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "the hash of the block to mark as invalid"},
                }}
                .ToString() +
            "\nResult:\n"
            "\nExamples:\n" +
            HelpExampleCli("invalidateblock", "\"blockhash\"") +
            HelpExampleRpc("invalidateblock", "\"blockhash\""));
    }

    const BlockHash hash(ParseHashV(request.params[0], "blockhash"));
    CValidationState state;

    CBlockIndex *pblockindex;
    {
        LOCK(cs_main);
        pblockindex = LookupBlockIndex(hash);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
    }
    InvalidateBlock(config, state, pblockindex);

    if (state.IsValid()) {
        ActivateBestChain(config, state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, FormatStateMessage(state));
    }

    return UniValue();
}

UniValue parkblock(const Config &config, const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error("parkblock \"blockhash\"\n"
                                 "\nMarks a block as parked.\n"
                                 "\nArguments:\n"
                                 "1. \"blockhash\"   (string, required) the "
                                 "hash of the block to park\n"
                                 "\nResult:\n"
                                 "\nExamples:\n" +
                                 HelpExampleCli("parkblock", "\"blockhash\"") +
                                 HelpExampleRpc("parkblock", "\"blockhash\""));
    }

    const BlockHash hash(ParseHashV(request.params[0], "blockhash"));
    CValidationState state;

    CBlockIndex *pblockindex;
    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }

        pblockindex = mapBlockIndex[hash];
    }
    ParkBlock(config, state, pblockindex);

    if (state.IsValid()) {
        ActivateBestChain(config, state);
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return UniValue();
}

static UniValue reconsiderblock(const Config &config,
                                const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"reconsiderblock",
                "\nRemoves invalidity status of a block and its descendants, reconsider them for activation.\n"
                "This can be used to undo the effects of invalidateblock.\n",
                {
                    {"blockhash", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "the hash of the block to reconsider"},
                }}
                .ToString() +
            "\nResult:\n"
            "\nExamples:\n" +
            HelpExampleCli("reconsiderblock", "\"blockhash\"") +
            HelpExampleRpc("reconsiderblock", "\"blockhash\""));
    }

    const BlockHash hash(ParseHashV(request.params[0], "blockhash"));

    {
        LOCK(cs_main);
        CBlockIndex *pblockindex = LookupBlockIndex(hash);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }

        ResetBlockFailureFlags(pblockindex);
    }

    CValidationState state;
    ActivateBestChain(config, state);

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, FormatStateMessage(state));
    }

    return UniValue();
}

UniValue unparkblock(const Config &config, const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "unparkblock \"blockhash\"\n"
            "\nRemoves parked status of a block and its descendants, "
            "reconsider them for activation.\n"
            "This can be used to undo the effects of parkblock.\n"
            "\nArguments:\n"
            "1. \"blockhash\"   (string, required) the hash of the block to "
            "unpark\n"
            "\nResult:\n"
            "\nExamples:\n" +
            HelpExampleCli("unparkblock", "\"blockhash\"") +
            HelpExampleRpc("unparkblock", "\"blockhash\""));
    }

    const BlockHash hash(ParseHashV(request.params[0], "blockhash"));

    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }

        CBlockIndex *pblockindex = mapBlockIndex[hash];
        UnparkBlockAndChildren(pblockindex);
    }

    CValidationState state;
    ActivateBestChain(config, state);

    if (!state.IsValid()) {
        throw JSONRPCError(RPC_DATABASE_ERROR, state.GetRejectReason());
    }

    return UniValue();
}

static UniValue getchaintxstats(const Config &config,
                                const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"getchaintxstats",
                "\nCompute statistics about the total number and rate of transactions in the chain.\n",
                {
                    {"nblocks", RPCArg::Type::NUM, /* opt */ true, /* default_val */ "one month", "Size of the window in number of blocks"},
                    {"blockhash", RPCArg::Type::STR_HEX, /* opt */ true, /* default_val */ "", "The hash of the block that ends the window."},
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"time\": xxxxx,                         (numeric) The "
            "timestamp for the final block in the window in UNIX format.\n"
            "  \"txcount\": xxxxx,                      (numeric) The total "
            "number of transactions in the chain up to that point.\n"
            "  \"window_final_block_hash\": \"...\",      (string) The hash of "
            "the final block in the window.\n"
            "  \"window_block_count\": xxxxx,           (numeric) Size of "
            "the window in number of blocks.\n"
            "  \"window_tx_count\": xxxxx,              (numeric) The number "
            "of transactions in the window. Only returned if "
            "\"window_block_count\" is > 0.\n"
            "  \"window_interval\": xxxxx,              (numeric) The elapsed "
            "time in the window in seconds. Only returned if "
            "\"window_block_count\" is > 0.\n"
            "  \"txrate\": x.xx,                        (numeric) The average "
            "rate of transactions per second in the window. Only returned if "
            "\"window_interval\" is > 0.\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getchaintxstats", "") +
            HelpExampleRpc("getchaintxstats", "2016"));
    }

    const CBlockIndex *pindex;

    // By default: 1 month
    int blockcount = 30 * 24 * 60 * 60 /
                     config.GetChainParams().GetConsensus().nPowTargetSpacing;

    if (request.params[1].isNull()) {
        LOCK(cs_main);
        pindex = ::ChainActive().Tip();
    } else {
        BlockHash hash(ParseHashV(request.params[1], "blockhash"));
        LOCK(cs_main);
        pindex = LookupBlockIndex(hash);
        if (!pindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
        if (!::ChainActive().Contains(pindex)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                               "Block is not in main chain");
        }
    }

    assert(pindex != nullptr);

    if (request.params[0].isNull()) {
        blockcount = std::max(0, std::min(blockcount, pindex->nHeight - 1));
    } else {
        blockcount = request.params[0].get_int();

        if (blockcount < 0 ||
            (blockcount > 0 && blockcount >= pindex->nHeight)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid block count: "
                                                      "should be between 0 and "
                                                      "the block's height - 1");
        }
    }

    const CBlockIndex *pindexPast =
        pindex->GetAncestor(pindex->nHeight - blockcount);
    int nTimeDiff =
        pindex->GetMedianTimePast() - pindexPast->GetMedianTimePast();
    int nTxDiff = pindex->GetChainTxCount() - pindexPast->GetChainTxCount();

    UniValue::Object ret;
    ret.reserve(blockcount > 0 ? nTimeDiff > 0 ? 7 : 6 : 4);
    ret.emplace_back("time", pindex->GetBlockTime());
    ret.emplace_back("txcount", pindex->GetChainTxCount());
    ret.emplace_back("window_final_block_hash", pindex->GetBlockHash().GetHex());
    ret.emplace_back("window_block_count", blockcount);
    if (blockcount > 0) {
        ret.emplace_back("window_tx_count", nTxDiff);
        ret.emplace_back("window_interval", nTimeDiff);
        if (nTimeDiff > 0) {
            ret.emplace_back("txrate", double(nTxDiff) / nTimeDiff);
        }
    }
    return ret;
}

template <typename T>
static T CalculateTruncatedMedian(std::vector<T> &scores) {
    size_t size = scores.size();
    if (size == 0) {
        return T();
    }

    std::sort(scores.begin(), scores.end());
    if (size % 2 == 0) {
        return (scores[size / 2 - 1] + scores[size / 2]) / 2;
    } else {
        return scores[size / 2];
    }
}

void CalculatePercentilesBySize(Amount result[NUM_GETBLOCKSTATS_PERCENTILES], std::vector<std::pair<Amount, int64_t>>& scores, int64_t total_size)
{
    if (scores.empty()) {
        return;
    }

    std::sort(scores.begin(), scores.end());

    // 10th, 25th, 50th, 75th, and 90th percentile weight units.
    const double weights[NUM_GETBLOCKSTATS_PERCENTILES] = {
        total_size / 10.0, total_size / 4.0, total_size / 2.0, (total_size * 3.0) / 4.0, (total_size * 9.0) / 10.0
    };

    int64_t next_percentile_index = 0;
    int64_t cumulative_weight = 0;
    for (const auto& element : scores) {
        cumulative_weight += element.second;
        while (next_percentile_index < NUM_GETBLOCKSTATS_PERCENTILES && cumulative_weight >= weights[next_percentile_index]) {
            result[next_percentile_index] = element.first;
            ++next_percentile_index;
        }
    }

    // Fill any remaining percentiles with the last value.
    for (int64_t i = next_percentile_index; i < NUM_GETBLOCKSTATS_PERCENTILES; i++) {
        result[i] = scores.back().first;
    }
}

template <typename T> static inline bool SetHasKeys(const std::set<T> &set) {
    return false;
}
template <typename T, typename Tk, typename... Args>
static inline bool SetHasKeys(const std::set<T> &set, const Tk &key,
                              const Args &... args) {
    return (set.count(key) != 0) || SetHasKeys(set, args...);
}

// outpoint (needed for the utxo index) + nHeight + fCoinBase
static constexpr size_t PER_UTXO_OVERHEAD =
    sizeof(COutPoint) + sizeof(uint32_t) + sizeof(bool);

/// Lock-free -- will throw if undo rev??.dat file not found or was pruned, etc.
/// Guaranteed to return a valid undo or fail.
static CBlockUndo ReadUndoChecked(const CBlockIndex *pblockindex) {
    CBlockUndo undo;
    auto doRead = [&] {
        // Note: we special-case block 0 to preserve RPC compatibility with previous
        // incarnations of `getblockstats` that did not use the undo mechanism to grab
        // stats. Those earlier versions would return stats for block 0. So, we return
        // empty undo for genesis (genesis has no actual undo file on disk but an empty
        // CBlockUndo is a perfect simulacrum of its undo file if it were to have one)
        if (pblockindex->nHeight != 0 && !UndoReadFromDisk(undo, pblockindex)) {
            // Undo not found on disk. This could be because we have the block
            // header in our index but don't have the block (for example if a
            // non-whitelisted node sends us an unrequested long chain of valid
            // blocks, we add the headers to our index, but don't accept the block).
            // This can also happen if in the extremely rare event that the undo file
            // was pruned from underneath us as we were executing getblockstats().
            throw JSONRPCError(RPC_MISC_ERROR, "Can't read undo data from disk");
        }
    };
    if (fPruneMode) {
        // Note: in pruned mode we must take cs_main here because it's possible for FlushStateToDisk()
        // in validation.cpp to also attempt to remove this file while we have it open.  This is not
        // normally a problem except for on Windows, where FlushStateToDisk() would fail to remove the
        // undo file we have open here, in which case on Windows the node would AbortNode().  Hence
        // the need for this locking in the fPrunedMode case only.
        LOCK(cs_main);
        doRead();
    } else {
        // Non-pruned mode, we can benefit from not having to grab cs_main here since undos never
        // go away -- this increases parallelism in the case of non-pruning nodes.
        doRead();
    }

    return undo;
}

static UniValue getblockstats(const Config &config,
                              const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 4) {
        throw std::runtime_error(
            RPCHelpMan{"getblockstats",
                "\nCompute per block statistics for a given window. All amounts are in "
                + CURRENCY_UNIT + ".\n"
                "It won't work for some heights with pruning.\n",
                {
                    {"hash_or_height", RPCArg::Type::NUM, /* opt */ false, /* default_val */ "", "The block hash or height of the target block", "", {"", "string or numeric"}},
                    {"stats", RPCArg::Type::ARR, /* opt */ true, /* default_val */ "", "Values to plot, by default all values (see result below)",
                        {
                            {"height", RPCArg::Type::STR, /* opt */ true, /* default_val */ "", "Selected statistic"},
                            {"time", RPCArg::Type::STR, /* opt */ true, /* default_val */ "", "Selected statistic"},
                        },
                        "stats"},
                }}
                .ToString() +
            "\nResult:\n"
            "{                           (json object)\n"
            "  \"avgfee\": x.xxx,          (numeric) Average fee in the block\n"
            "  \"avgfeerate\": x.xxx,      (numeric) Average feerate (in " +
            CURRENCY_UNIT +
            " per byte)\n"
            "  \"avgtxsize\": xxxxx,       (numeric) Average transaction size\n"
            "  \"blockhash\": xxxxx,       (string) The block hash (to check for potential reorgs)\n"
            "  \"feerate_percentiles\": [  (array of numeric) Feerates at the 10th, 25th, 50th, 75th, and 90th percentile weight unit (in satoshis per byte)\n"
            "      \"10th_percentile_feerate\",      (numeric) The 10th percentile feerate\n"
            "      \"25th_percentile_feerate\",      (numeric) The 25th percentile feerate\n"
            "      \"50th_percentile_feerate\",      (numeric) The 50th percentile feerate\n"
            "      \"75th_percentile_feerate\",      (numeric) The 75th percentile feerate\n"
            "      \"90th_percentile_feerate\",      (numeric) The 90th percentile feerate\n"
            "  ],\n"
            "  \"height\": xxxxx,          (numeric) The height of the block\n"
            "  \"ins\": xxxxx,             (numeric) The number of inputs "
            "(excluding coinbase)\n"
            "  \"maxfee\": xxxxx,          (numeric) Maximum fee in the block\n"
            "  \"maxfeerate\": xxxxx,      (numeric) Maximum feerate (in " +
            CURRENCY_UNIT +
            " per byte)\n"
            "  \"maxtxsize\": xxxxx,       (numeric) Maximum transaction size\n"
            "  \"medianfee\": x.xxx,       (numeric) Truncated median fee in "
            "the block\n"
            "  \"mediantime\": xxxxx,      (numeric) The block median time "
            "past\n"
            "  \"mediantxsize\": xxxxx,    (numeric) Truncated median "
            "transaction size\n"
            "  \"minfee\": x.xxx,          (numeric) Minimum fee in the block\n"
            "  \"minfeerate\": xx.xx,      (numeric) Minimum feerate (in " +
            CURRENCY_UNIT +
            " per byte)\n"
            "  \"mintxsize\": xxxxx,       (numeric) Minimum transaction size\n"
            "  \"outs\": xxxxx,            (numeric) The number of outputs\n"
            "  \"subsidy\": x.xxx,         (numeric) The block subsidy\n"
            "  \"time\": xxxxx,            (numeric) The block time\n"
            "  \"total_out\": x.xxx,       (numeric) Total amount in all "
            "outputs (excluding coinbase and thus reward [ie subsidy + "
            "totalfee])\n"
            "  \"total_size\": xxxxx,      (numeric) Total size of all "
            "non-coinbase transactions\n"
            "  \"totalfee\": x.xxx,        (numeric) The fee total\n"
            "  \"txs\": xxxxx,             (numeric) The number of "
            "transactions (excluding coinbase)\n"
            "  \"utxo_increase\": xxxxx,   (numeric) The increase/decrease in "
            "the number of unspent outputs\n"
            "  \"utxo_size_inc\": xxxxx,   (numeric) The increase/decrease in "
            "size for the utxo index (not discounting op_return and similar)\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getblockstats",
                           "1000 '[\"minfeerate\",\"avgfeerate\"]'") +
            HelpExampleRpc("getblockstats",
                           "1000 '[\"minfeerate\",\"avgfeerate\"]'"));
    }

    CBlockIndex *pindex{};

    {
        LOCK(cs_main);

        if (request.params[0].isNum()) {
            const int height = request.params[0].get_int();
            const int current_tip = ::ChainActive().Height();
            if (height < 0) {
                throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    strprintf("Target block height %d is negative", height));
            }
            if (height > current_tip) {
                throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    strprintf("Target block height %d after current tip %d", height,
                              current_tip));
            }

            pindex = ::ChainActive()[height];
        } else {
            const BlockHash hash(ParseHashV(request.params[0], "hash_or_height"));
            pindex = LookupBlockIndex(hash);
            if (!pindex) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
            }
            if (!::ChainActive().Contains(pindex)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                                   strprintf("Block is not in chain %s",
                                             Params().NetworkIDString()));
            }
        }
        assert(pindex != nullptr);
        ThrowIfPrunedBlock(pindex);
    }
    // Note: all of the below code has been verified to not require cs_main


    std::set<std::string> stats;
    if (!request.params[1].isNull()) {
        for (const UniValue& stat : request.params[1].get_array()) {
            stats.insert(stat.get_str());
        }
    }

    const CBlock block = ReadBlockChecked(config, pindex);

    // Calculate everything if nothing selected (default)
    const bool do_all = stats.size() == 0;
    const bool do_mediantxsize = do_all || stats.count("mediantxsize") != 0;
    const bool do_medianfee = do_all || stats.count("medianfee") != 0;
    const bool do_feerate_percentiles = do_all || stats.count("feerate_percentiles") != 0;
    const bool loop_inputs =
        do_all || do_medianfee || do_feerate_percentiles ||
        SetHasKeys(stats, "utxo_size_inc", "totalfee", "avgfee", "avgfeerate",
                   "minfee", "maxfee", "minfeerate", "maxfeerate");
    const bool loop_outputs = do_all || loop_inputs || stats.count("total_out");
    const bool do_calculate_size =
        do_mediantxsize || loop_inputs ||
        SetHasKeys(stats, "total_size", "avgtxsize", "mintxsize", "maxtxsize");

    const int64_t excessiveBlockSize = config.GetExcessiveBlockSize();
    Amount maxfee = Amount::zero();
    Amount maxfeerate = Amount::zero();
    Amount minfee = MAX_MONEY;
    Amount minfeerate = MAX_MONEY;
    Amount total_out = Amount::zero();
    Amount totalfee = Amount::zero();
    int64_t inputs = 0;
    int64_t maxtxsize = 0;
    int64_t mintxsize = excessiveBlockSize;
    int64_t outputs = 0;
    int64_t total_size = 0;
    int64_t utxo_size_inc = 0;
    std::vector<Amount> fee_array;
    std::vector<std::pair<Amount, int64_t>> feerate_array;
    std::vector<int64_t> txsize_array;

    // read the undo file so we can calculate fees -- but only if loop_inputs is true
    // (since if it's false we won't need this data and we shouldn't spend time deserializing it)
    const CBlockUndo &&blockUndo = loop_inputs ? ReadUndoChecked(pindex) : CBlockUndo{};

    // Reserve for the above vectors only if we use them
    if (do_mediantxsize) txsize_array.reserve(block.vtx.size());
    if (do_medianfee) fee_array.reserve(block.vtx.size());
    if (do_feerate_percentiles) feerate_array.reserve(block.vtx.size());

    for (size_t i_tx = 0; i_tx < block.vtx.size(); ++i_tx) {
        const auto &tx = block.vtx[i_tx];
        outputs += tx->vout.size();
        Amount tx_total_out = Amount::zero();
        if (loop_outputs) {
            for (const CTxOut &out : tx->vout) {
                tx_total_out += out.nValue;
                utxo_size_inc +=
                    GetSerializeSize(out, PROTOCOL_VERSION) + PER_UTXO_OVERHEAD;
            }
        }

        if (tx->IsCoinBase()) {
            continue;
        }

        // Don't count coinbase's fake input
        inputs += tx->vin.size();
        // Don't count coinbase reward
        total_out += tx_total_out;

        int64_t tx_size = 0;
        if (do_calculate_size) {
            tx_size = tx->GetTotalSize();
            if (do_mediantxsize) {
                txsize_array.push_back(tx_size);
            }
            maxtxsize = std::max(maxtxsize, tx_size);
            mintxsize = std::min(mintxsize, tx_size);
            total_size += tx_size;
        }

        if (loop_inputs) {
            Amount tx_total_in = Amount::zero();
            const auto &txundo = blockUndo.vtxundo.at(i_tx - 1); // checked access here, guard against programming errors
            // We use the block undo info to find the inputs to this tx and use that information to calculate fees
            for (const Coin &coin : txundo.vprevout) {
                const CTxOut &prevoutput = coin.GetTxOut();

                tx_total_in += prevoutput.nValue;
                utxo_size_inc -=
                    GetSerializeSize(prevoutput, PROTOCOL_VERSION) +
                    PER_UTXO_OVERHEAD;
            }

            Amount txfee = tx_total_in - tx_total_out;
            assert(MoneyRange(txfee));
            if (do_medianfee) {
                fee_array.push_back(txfee);
            }
            maxfee = std::max(maxfee, txfee);
            minfee = std::min(minfee, txfee);
            totalfee += txfee;

            Amount feerate = tx_size ? txfee / tx_size : Amount::zero();
            if (do_feerate_percentiles) {
                feerate_array.emplace_back(feerate, tx_size);
            }
            maxfeerate = std::max(maxfeerate, feerate);
            minfeerate = std::min(minfeerate, feerate);
        }
    }

    Amount feerate_percentiles[NUM_GETBLOCKSTATS_PERCENTILES] = { Amount::zero() };
    CalculatePercentilesBySize(feerate_percentiles, feerate_array, total_size);

    UniValue::Array feerates_res;
    feerates_res.reserve(NUM_GETBLOCKSTATS_PERCENTILES);
    for (int64_t i = 0; i < NUM_GETBLOCKSTATS_PERCENTILES; i++) {
        feerates_res.push_back(ValueFromAmount(feerate_percentiles[i]));
    }

    UniValue::Object ret;
    ret.reserve(25); // not critical but be sure to update this reserve size if adding/removing entries below.
    ret.emplace_back("avgfee",
                   ValueFromAmount((block.vtx.size() > 1)
                                       ? totalfee / int((block.vtx.size() - 1))
                                       : Amount::zero()));
    ret.emplace_back("avgfeerate",
                   ValueFromAmount((total_size > 0) ? totalfee / total_size
                                                    : Amount::zero()));
    ret.emplace_back("avgtxsize", (block.vtx.size() > 1)
                                    ? total_size / (block.vtx.size() - 1)
                                    : 0);
    ret.emplace_back("blockhash", pindex->GetBlockHash().GetHex());
    ret.emplace_back("feerate_percentiles", std::move(feerates_res));
    ret.emplace_back("height", pindex->nHeight);
    ret.emplace_back("ins", inputs);
    ret.emplace_back("maxfee", ValueFromAmount(maxfee));
    ret.emplace_back("maxfeerate", ValueFromAmount(maxfeerate));
    ret.emplace_back("maxtxsize", maxtxsize);
    ret.emplace_back("medianfee",
                   ValueFromAmount(CalculateTruncatedMedian(fee_array)));
    ret.emplace_back("mediantime", pindex->GetMedianTimePast());
    ret.emplace_back("mediantxsize", CalculateTruncatedMedian(txsize_array));
    ret.emplace_back(
        "minfee",
        ValueFromAmount((minfee == MAX_MONEY) ? Amount::zero() : minfee));
    ret.emplace_back("minfeerate",
                   ValueFromAmount((minfeerate == MAX_MONEY) ? Amount::zero()
                                                             : minfeerate));
    ret.emplace_back("mintxsize", mintxsize == excessiveBlockSize ? 0 : mintxsize);
    ret.emplace_back("outs", outputs);
    ret.emplace_back("subsidy", ValueFromAmount(GetBlockSubsidy(
                                  pindex->nHeight, Params().GetConsensus())));
    ret.emplace_back("time", pindex->GetBlockTime());
    ret.emplace_back("total_out", ValueFromAmount(total_out));
    ret.emplace_back("total_size", total_size);
    ret.emplace_back("totalfee", ValueFromAmount(totalfee));
    ret.emplace_back("txs", block.vtx.size());
    ret.emplace_back("utxo_increase", outputs - inputs);
    ret.emplace_back("utxo_size_inc", utxo_size_inc);

    if (!do_all) {
        // in this branch, we must return only the keys the client asked for
        UniValue::Object selected;
        selected.reserve(stats.size());
        for (const std::string &stat : stats) {
            UniValue *value = ret.locate(stat);
            if (!value || value->isNull()) {
                throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    strprintf("Invalid selected statistic %s", stat));
            }
            selected.emplace_back(stat, std::move(*value));
        }
        return selected;
    }

    return ret; // compiler will invoke Univalue(Univalue::Object &&) move-constructor.
}

static UniValue savemempool(const Config &config,
                            const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            RPCHelpMan{"savemempool",
                "\nDumps the mempool to disk. It will fail until the previous dump is fully loaded.\n", {}}
                .ToString() +
            "\nExamples:\n"
            + HelpExampleCli("savemempool", "")
            + HelpExampleRpc("savemempool", "")
        );
    }

    if (!::g_mempool.IsLoaded()) {
        throw JSONRPCError(RPC_MISC_ERROR, "The mempool was not loaded yet");
    }

    if (!DumpMempool(::g_mempool)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Unable to dump mempool to disk");
    }

    return UniValue();
}

//! Search for a given set of pubkey scripts
static bool FindScriptPubKey(std::atomic<int> &scan_progress,
                             const std::atomic<bool> &should_abort,
                             int64_t &count, CCoinsViewCursor *cursor,
                             const std::set<CScript> &needles,
                             std::map<COutPoint, Coin> &out_results) {
    scan_progress = 0;
    count = 0;
    while (cursor->Valid()) {
        COutPoint key;
        Coin coin;
        if (!cursor->GetKey(key) || !cursor->GetValue(coin)) {
            return false;
        }
        if (++count % 8192 == 0) {
            boost::this_thread::interruption_point();
            if (should_abort) {
                // allow to abort the scan via the abort reference
                return false;
            }
        }
        if (count % 256 == 0) {
            // update progress reference every 256 item
            const TxId &txid = key.GetTxId();
            uint32_t high = 0x100 * *txid.begin() + *(txid.begin() + 1);
            scan_progress = int(high * 100.0 / 65536.0 + 0.5);
        }
        if (needles.count(coin.GetTxOut().scriptPubKey)) {
            out_results.emplace(key, coin);
        }
        cursor->Next();
    }
    scan_progress = 100;
    return true;
}

/** RAII object to prevent concurrency issue when scanning the txout set */
static std::mutex g_utxosetscan;
static std::atomic<int> g_scan_progress;
static std::atomic<bool> g_scan_in_progress;
static std::atomic<bool> g_should_abort_scan;
class CoinsViewScanReserver {
private:
    bool m_could_reserve;

public:
    explicit CoinsViewScanReserver() : m_could_reserve(false) {}

    bool reserve() {
        assert(!m_could_reserve);
        std::lock_guard<std::mutex> lock(g_utxosetscan);
        if (g_scan_in_progress) {
            return false;
        }
        g_scan_in_progress = true;
        m_could_reserve = true;
        return true;
    }

    ~CoinsViewScanReserver() {
        if (m_could_reserve) {
            std::lock_guard<std::mutex> lock(g_utxosetscan);
            g_scan_in_progress = false;
        }
    }
};

static UniValue scantxoutset(const Config &config,
                             const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"scantxoutset",
                "\nEXPERIMENTAL warning: this call may be removed or changed in future releases.\n"
                "\nScans the unspent transaction output set for entries that match certain output descriptors.\n"
                "Examples of output descriptors are:\n"
                "    addr(<address>)                      Outputs whose scriptPubKey corresponds to the specified address (does not include P2PK)\n"
                "    raw(<hex script>)                    Outputs whose scriptPubKey equals the specified hex scripts\n"
                "    combo(<pubkey>)                      P2PK and P2PKH outputs for the given pubkey\n"
                "    pkh(<pubkey>)                        P2PKH outputs for the given pubkey\n"
                "    sh(multi(<n>,<pubkey>,<pubkey>,...)) P2SH-multisig outputs for the given threshold and pubkeys\n"
                "\nIn the above, <pubkey> either refers to a fixed public key in hexadecimal notation, or to an xpub/xprv optionally followed by one\n"
                "or more path elements separated by \"/\", and optionally ending in \"/*\" (unhardened), or \"/*'\" or \"/*h\" (hardened) to specify all\n"
                "unhardened or hardened child keys.\n"
                "In the latter case, a range needs to be specified by below if different from 1000.\n"
                "For more information on output descriptors, see the documentation in the doc/descriptors.md file.\n",
                {
                    {"action", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "The action to execute\n"
            "                                      \"start\" for starting a scan\n"
            "                                      \"abort\" for aborting the current scan (returns true when abort was successful)\n"
            "                                      \"status\" for progress report (in %) of the current scan"},
                    {"scanobjects", RPCArg::Type::ARR, /* opt */ false, /* default_val */ "", "Array of scan objects\n"
            "                                  Every scan object is either a string descriptor or an object:",
                        {
                            {"descriptor", RPCArg::Type::STR, /* opt */ true, /* default_val */ "", "An output descriptor"},
                            {"", RPCArg::Type::OBJ, /* opt */ true, /* default_val */ "", "An object with output descriptor and metadata",
                                {
                                    {"desc", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "An output descriptor"},
                                    {"range", RPCArg::Type::NUM, /* opt */ true, /* default_val */ "1000", "Up to what child index HD chains should be explored"},
                                },
                            },
                        },
                        "[scanobjects,...]"},
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"unspents\": [\n"
            "    {\n"
            "    \"txid\" : \"transactionid\",     (string) The transaction "
            "id\n"
            "    \"vout\": n,                    (numeric) the vout value\n"
            "    \"scriptPubKey\" : \"script\",    (string) the script key\n"
            "    \"amount\" : x.xxx,             (numeric) The total amount "
            "in " +
            CURRENCY_UNIT +
            " of the unspent output\n"
            "    \"height\" : n,                 (numeric) Height of the "
            "unspent transaction output\n"
            "   }\n"
            "   ,...],\n"
            " \"total_amount\" : x.xxx,          (numeric) The total amount of "
            "all found unspent outputs in " +
            CURRENCY_UNIT +
            "\n"
            "]\n");
    }

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VARR});

    if (request.params[0].get_str() == "status") {
        CoinsViewScanReserver reserver;
        if (reserver.reserve()) {
            // no scan in progress
            return UniValue();
        }
        UniValue::Object result;
        result.reserve(1);
        result.emplace_back("progress", g_scan_progress.load());
        return result;
    }

    if (request.params[0].get_str() == "abort") {
        CoinsViewScanReserver reserver;
        if (reserver.reserve()) {
            // reserve was possible which means no scan was running
            return false;
        }
        // set the abort flag
        g_should_abort_scan = true;
        return true;
    }

    if (request.params[0].get_str() == "start") {
        CoinsViewScanReserver reserver;
        if (!reserver.reserve()) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "Scan already in progress, use action \"abort\" or \"status\"");
        }
        std::set<CScript> needles;
        Amount total_in = Amount::zero();

        // loop through the scan objects
        for (const UniValue &scanobject : request.params[1].get_array()) {
            std::string desc_str;
            int range = 1000;
            if (scanobject.isStr()) {
                desc_str = scanobject.get_str();
            } else if (scanobject.isObject()) {
                const UniValue & desc_uni = scanobject["desc"];
                if (desc_uni.isNull()) {
                    throw JSONRPCError(
                        RPC_INVALID_PARAMETER,
                        "Descriptor needs to be provided in scan object");
                }
                desc_str = desc_uni.get_str();
                const UniValue & range_uni = scanobject["range"];
                if (!range_uni.isNull()) {
                    range = range_uni.get_int();
                    if (range < 0 || range > 1000000) {
                        throw JSONRPCError(RPC_INVALID_PARAMETER,
                                           "range out of range");
                    }
                }
            } else {
                throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    "Scan object needs to be either a string or an object");
            }

            FlatSigningProvider provider;
            auto desc = Parse(desc_str, provider);
            if (!desc) {
                throw JSONRPCError(
                    RPC_INVALID_ADDRESS_OR_KEY,
                    strprintf("Invalid descriptor '%s'", desc_str));
            }
            if (!desc->IsRange()) {
                range = 0;
            }
            for (int i = 0; i <= range; ++i) {
                std::vector<CScript> scripts;
                if (!desc->Expand(i, provider, scripts, provider)) {
                    throw JSONRPCError(
                        RPC_INVALID_ADDRESS_OR_KEY,
                        strprintf(
                            "Cannot derive script without private keys: '%s'",
                            desc_str));
                }
                needles.insert(scripts.begin(), scripts.end());
            }
        }

        // Scan the unspent transaction output set for inputs
        std::map<COutPoint, Coin> coins;
        g_should_abort_scan = false;
        g_scan_progress = 0;
        int64_t count = 0;
        std::unique_ptr<CCoinsViewCursor> pcursor;
        {
            LOCK(cs_main);
            FlushStateToDisk();
            pcursor = std::unique_ptr<CCoinsViewCursor>(pcoinsdbview->Cursor());
            assert(pcursor);
        }
        bool res = FindScriptPubKey(g_scan_progress, g_should_abort_scan, count,
                                    pcursor.get(), needles, coins);
        UniValue::Object result;
        result.reserve(4);
        result.emplace_back("success", res);
        result.emplace_back("searched_items", count);

        UniValue::Array unspents;
        unspents.reserve(coins.size());

        for (const auto &it : coins) {
            const COutPoint &outpoint = it.first;
            const Coin &coin = it.second;
            const CTxOut &txo = coin.GetTxOut();
            total_in += txo.nValue;

            UniValue::Object unspent;
            unspent.reserve(5);
            unspent.emplace_back("txid", outpoint.GetTxId().GetHex());
            unspent.emplace_back("vout", outpoint.GetN());
            unspent.emplace_back("scriptPubKey", HexStr(txo.scriptPubKey));
            unspent.emplace_back("amount", ValueFromAmount(txo.nValue));
            unspent.emplace_back("height", coin.GetHeight());

            unspents.emplace_back(std::move(unspent));
        }
        result.emplace_back("unspents", std::move(unspents));
        result.emplace_back("total_amount", ValueFromAmount(total_in));
        return result;
    }

    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid command");
}

// clang-format off
static const ContextFreeRPCCommand commands[] = {
    //  category            name                      actor (function)        argNames
    //  ------------------- ------------------------  ----------------------  ----------
    { "blockchain",         "finalizeblock",          finalizeblock,          {"blockhash"} },
    { "blockchain",         "getbestblockhash",       getbestblockhash,       {} },
    { "blockchain",         "getblock",               getblock,               {"blockhash","verbosity|verbose"} },
    { "blockchain",         "getblockchaininfo",      getblockchaininfo,      {} },
    { "blockchain",         "getblockcount",          getblockcount,          {} },
    { "blockchain",         "getblockhash",           getblockhash,           {"height"} },
    { "blockchain",         "getblockheader",         getblockheader,         {"blockhash|hash_or_height","verbose"} },
    { "blockchain",         "getblockstats",          getblockstats,          {"hash_or_height","stats"} },
    { "blockchain",         "getchaintips",           getchaintips,           {} },
    { "blockchain",         "getchaintxstats",        getchaintxstats,        {"nblocks", "blockhash"} },
    { "blockchain",         "getdifficulty",          getdifficulty,          {} },
    { "blockchain",         "getfinalizedblockhash",  getfinalizedblockhash,  {} },
    { "blockchain",         "getmempoolancestors",    getmempoolancestors,    {"txid","verbose"} },
    { "blockchain",         "getmempooldescendants",  getmempooldescendants,  {"txid","verbose"} },
    { "blockchain",         "getmempoolentry",        getmempoolentry,        {"txid"} },
    { "blockchain",         "getmempoolinfo",         getmempoolinfo,         {} },
    { "blockchain",         "getrawmempool",          getrawmempool,          {"verbose"} },
    { "blockchain",         "gettxout",               gettxout,               {"txid","n","include_mempool"} },
    { "blockchain",         "gettxoutsetinfo",        gettxoutsetinfo,        {} },
    { "blockchain",         "invalidateblock",        invalidateblock,        {"blockhash"} },
    { "blockchain",         "parkblock",              parkblock,              {"blockhash"} },
    { "blockchain",         "preciousblock",          preciousblock,          {"blockhash"} },
    { "blockchain",         "pruneblockchain",        pruneblockchain,        {"height"} },
    { "blockchain",         "reconsiderblock",        reconsiderblock,        {"blockhash"} },
    { "blockchain",         "savemempool",            savemempool,            {} },
    { "blockchain",         "scantxoutset",           scantxoutset,           {"action", "scanobjects"} },
    { "blockchain",         "unparkblock",            unparkblock,            {"blockhash"} },
    { "blockchain",         "verifychain",            verifychain,            {"checklevel","nblocks"} },

    /* Not shown in help */
    { "hidden",             "syncwithvalidationinterfacequeue", syncwithvalidationinterfacequeue, {} },
    { "hidden",             "waitforblock",                     waitforblock,                     {"blockhash","timeout"} },
    { "hidden",             "waitforblockheight",               waitforblockheight,               {"height","timeout"} },
    { "hidden",             "waitfornewblock",                  waitfornewblock,                  {"timeout"} },
};
// clang-format on

void RegisterBlockchainRPCCommands(CRPCTable &t) {
    for (unsigned int vcidx = 0; vcidx < std::size(commands); ++vcidx) {
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
    }
}
