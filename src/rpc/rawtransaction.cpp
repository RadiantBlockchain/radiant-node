// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <compat/byteswap.h>
#include <config.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <index/txindex.h>
#include <init.h>
#include <key_io.h>
#include <keystore.h>
#include <merkleblock.h>
#include <node/transaction.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <psbt.h>
#include <rpc/rawtransaction.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/sign.h>
#include <script/standard.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <validation.h>
#include <validationinterface.h>

#include <cstdint>

#include <univalue.h>

static UniValue::Object TxToJSON(const Config &config, const CTransaction &tx, const BlockHash &hashBlock) {
    // Call into TxToUniv() in bitcoin-common to decode the transaction hex.
    //
    // Blockchain contextual information (confirmations and blocktime) is not
    // available to code in bitcoin-common, so we query them here and push the
    // data into the returned UniValue.
    UniValue::Object entry = TxToUniv(config, tx, uint256(), true, RPCSerializationFlags());

    if (!hashBlock.IsNull()) {
        LOCK(cs_main);

        CBlockIndex *pindex = LookupBlockIndex(hashBlock);
        bool blockFound = pindex;
        bool inActiveChain = blockFound && ::ChainActive().Contains(pindex);
        entry.reserve(entry.size() + (blockFound ? inActiveChain ? 4 : 2 : 1));
        entry.emplace_back("blockhash", hashBlock.GetHex());
        if (blockFound) {
            if (inActiveChain) {
                entry.emplace_back("confirmations", 1 + ::ChainActive().Height() - pindex->nHeight);
                entry.emplace_back("time", pindex->GetBlockTime());
                entry.emplace_back("blocktime", pindex->GetBlockTime());
            } else {
                entry.emplace_back("confirmations", 0);
            }
        }
    }

    return entry;
}

static UniValue getrawtransaction(const Config &config,
                                  const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 3) {
        throw std::runtime_error(
            RPCHelpMan{"getrawtransaction",
                "\nNOTE: By default this function only works for mempool transactions. If the -txindex option is\n"
                "enabled, it also works for blockchain transactions. If the block which contains the transaction\n"
                "is known, its hash can be provided even for nodes without -txindex. Note that if a blockhash is\n"
                "provided, only that block will be searched and if the transaction is in the mempool or other\n"
                "blocks, or if this node does not have the given block available, the transaction will not be found.\n"
            "DEPRECATED: for now, it also works for transactions with unspent outputs.\n"

            "\nReturn the raw transaction data.\n"
            "\nIf verbose is 'true', returns an Object with information about 'txid'.\n"
            "If verbose is 'false' or omitted, returns a string that is serialized, hex-encoded data for 'txid'.\n"
                ,
                {
                    {"txid", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The transaction id"},
                    {"verbose", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "false", "If false, return a string, otherwise return a json object"},
                    {"blockhash", RPCArg::Type::STR_HEX, /* opt */ true, /* default_val */ "", "The block in which to look for the transaction"},
                }}
                .ToString() +
            "\nResult (if verbose is not set or set to false):\n"
            "\"data\"      (string) The serialized, hex-encoded data for "
            "'txid'\n"

            "\nResult (if verbose is set to true):\n"
            "{\n"
            "  \"hex\" : \"data\",       (string) The serialized, hex-encoded "
            "data for 'txid'\n"
            "  \"txid\" : \"id\",        (string) The transaction id (same as "
            "provided)\n"
            "  \"hash\" : \"id\",        (string) The transaction hash\n"
            "  \"size\" : n,             (numeric) The serialized transaction "
            "size\n"
            "  \"version\" : n,          (numeric) The version\n"
            "  \"locktime\" : ttt,       (numeric) The lock time\n"
            "  \"vin\" : [               (array of json objects)\n"
            "     {\n"
            "       \"txid\": \"id\",    (string) The transaction id\n"
            "       \"vout\": n,         (numeric)\n"
            "       \"scriptSig\": {     (json object) The script\n"
            "         \"asm\": \"asm\",  (string) asm\n"
            "         \"hex\": \"hex\"   (string) hex\n"
            "       },\n"
            "       \"sequence\": n      (numeric) The script sequence number\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"vout\" : [              (array of json objects)\n"
            "     {\n"
            "       \"value\" : x.xxx,            (numeric) The value in " +
            CURRENCY_UNIT +
            "\n"
            "       \"n\" : n,                    (numeric) index\n"
            "       \"scriptPubKey\" : {          (json object)\n"
            "         \"asm\" : \"asm\",          (string) the asm\n"
            "         \"hex\" : \"hex\",          (string) the hex\n"
            "         \"reqSigs\" : n,            (numeric) The required sigs\n"
            "         \"type\" : \"pubkeyhash\",  (string) The type, eg "
            "'pubkeyhash'\n"
            "         \"addresses\" : [           (json array of string)\n"
            "           \"address\"        (string) Radiant address\n"
            "           ,...\n"
            "         ]\n"
            "       }\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"blockhash\" : \"hash\",   (string) the block hash\n"
            "  \"confirmations\" : n,      (numeric) The confirmations\n"
            "  \"time\" : ttt,             (numeric) The transaction time in "
            "seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"blocktime\" : ttt,        (numeric) The block time in seconds "
            "since epoch (Jan 1 1970 GMT)\n"
            "  \"in_active_chain\": b  (bool) Whether specified block is in "
            "the active chain or not (only present with explicit \"blockhash\" "
            "argument)\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getrawtransaction", "\"mytxid\"") +
            HelpExampleCli("getrawtransaction", "\"mytxid\" true") +
            HelpExampleRpc("getrawtransaction", "\"mytxid\", true") +
            HelpExampleCli("getrawtransaction",
                           "\"mytxid\" false \"myblockhash\"") +
            HelpExampleCli("getrawtransaction",
                           "\"mytxid\" true \"myblockhash\""));
    }

    bool in_active_chain = true;
    TxId txid = TxId(ParseHashV(request.params[0], "parameter 1"));
    CBlockIndex *blockindex = nullptr;

    const CChainParams &params = config.GetChainParams();
    if (txid == params.GenesisBlock().hashMerkleRoot) {
        // Special exception for the genesis block coinbase transaction
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           "The genesis block coinbase is not considered an "
                           "ordinary transaction and cannot be retrieved");
    }

    // Accept either a bool (true) or a num (>=1) to indicate verbose output.
    bool fVerbose = false;
    if (!request.params[1].isNull()) {
        fVerbose = request.params[1].isNum()
                       ? request.params[1].get_int() != 0
                       : request.params[1].get_bool();
    }

    if (!request.params[2].isNull()) {
        LOCK(cs_main);

        BlockHash blockhash(ParseHashV(request.params[2], "parameter 3"));
        blockindex = LookupBlockIndex(blockhash);
        if (!blockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                               "Block hash not found");
        }
        in_active_chain = ::ChainActive().Contains(blockindex);
    }

    bool f_txindex_ready = false;
    if (g_txindex && !blockindex) {
        f_txindex_ready = g_txindex->BlockUntilSyncedToCurrentChain();
    }

    CTransactionRef tx;
    BlockHash hash_block;
    if (!GetTransaction(txid, tx, params.GetConsensus(), hash_block, true,
                        blockindex)) {
        std::string errmsg;
        if (blockindex) {
            if (!blockindex->nStatus.hasData()) {
                throw JSONRPCError(RPC_MISC_ERROR, "Block not available");
            }
            errmsg = "No such transaction found in the provided block";
        } else if (!g_txindex) {
            errmsg = "No such mempool transaction. Use -txindex to enable "
                     "blockchain transaction queries";
        } else if (!f_txindex_ready) {
            errmsg = "No such mempool transaction. Blockchain transactions are "
                     "still in the process of being indexed";
        } else {
            errmsg = "No such mempool or blockchain transaction";
        }
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           errmsg +
                               ". Use gettransaction for wallet transactions.");
    }

    if (!fVerbose) {
        return EncodeHexTx(*tx, RPCSerializationFlags());
    }

    UniValue::Object result = TxToJSON(config, *tx, hash_block);
    if (blockindex) {
        result.reserve(result.size() + 1);
        result.emplace_back("in_active_chain", in_active_chain);
    }
    return result;
}

static UniValue gettxoutproof(const Config &config,
                              const JSONRPCRequest &request) {
    if (request.fHelp ||
        (request.params.size() != 1 && request.params.size() != 2)) {
        throw std::runtime_error(
            RPCHelpMan{"gettxoutproof",
                "\nReturns a hex-encoded proof that \"txid\" was included in a block.\n"
                "\nNOTE: By default this function only works sometimes. This is when there is an\n"
                "unspent output in the utxo for this transaction. To make it always work,\n"
                "you need to maintain a transaction index, using the -txindex command line option or\n"
                "specify the block in which the transaction is included manually (by blockhash).\n",
                {
                    {"txids", RPCArg::Type::ARR, /* opt */ false, /* default_val */ "", "A json array of txids to filter",
                        {
                            {"txid", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "A transaction hash"},
                        },
                        },
                    {"blockhash", RPCArg::Type::STR_HEX, /* opt */ true, /* default_val */ "", "If specified, looks for txid in the block with this hash"},
                }}
                .ToString() +
            "\nResult:\n"
            "\"data\"           (string) A string that is a serialized, "
            "hex-encoded data for the proof.\n");
    }

    std::set<TxId> setTxIds;
    TxId oneTxId;
    const UniValue::Array& txids = request.params[0].get_array();
    for (const UniValue &utxid : txids) {
        TxId txid(ParseHashV(utxid, "txid"));
        if (setTxIds.count(txid)) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                std::string("Invalid parameter, duplicated txid: ") +
                    utxid.get_str());
        }

        setTxIds.insert(txid);
        oneTxId = txid;
    }

    CBlockIndex *pblockindex = nullptr;

    BlockHash hashBlock;
    if (!request.params[1].isNull()) {
        LOCK(cs_main);
        hashBlock = BlockHash(ParseHashV(request.params[1], "blockhash"));
        pblockindex = LookupBlockIndex(hashBlock);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
    } else {
        LOCK(cs_main);
        // Loop through txids and try to find which block they're in. Exit loop
        // once a block is found.
        for (const auto &txid : setTxIds) {
            const Coin &coin = AccessByTxid(*pcoinsTip, txid);
            if (!coin.IsSpent()) {
                pblockindex = ::ChainActive()[coin.GetHeight()];
                break;
            }
        }
    }

    // Allow txindex to catch up if we need to query it and before we acquire
    // cs_main.
    if (g_txindex && !pblockindex) {
        g_txindex->BlockUntilSyncedToCurrentChain();
    }

    const Consensus::Params &params = config.GetChainParams().GetConsensus();

    LOCK(cs_main);

    if (pblockindex == nullptr) {
        CTransactionRef tx;
        if (!GetTransaction(oneTxId, tx, params, hashBlock, false) ||
            hashBlock.IsNull()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                               "Transaction not yet in block");
        }

        pblockindex = LookupBlockIndex(hashBlock);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Transaction index corrupt");
        }
    }

    CBlock block;
    if (!ReadBlockFromDisk(block, pblockindex, params)) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");
    }

    unsigned int ntxFound = 0;
    for (const auto &tx : block.vtx) {
        if (setTxIds.count(tx->GetId())) {
            ntxFound++;
        }
    }

    if (ntxFound != setTxIds.size()) {
        throw JSONRPCError(
            RPC_INVALID_ADDRESS_OR_KEY,
            "Not all transactions found in specified or retrieved block");
    }

    CDataStream ssMB(SER_NETWORK, PROTOCOL_VERSION);
    CMerkleBlock mb(block, setTxIds);
    ssMB << mb;
    return HexStr(ssMB);
}

static UniValue verifytxoutproof(const Config &,
                                 const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"verifytxoutproof",
                "\nVerifies that a proof points to a transaction in a block, returning the transaction it commits to\n"
                "and throwing an RPC error if the block is not in our best chain\n",
                {
                    {"proof", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The hex-encoded proof generated by gettxoutproof"},
                }}
                .ToString() +
            "\nResult:\n"
            "[\"txid\"]      (array, strings) The txid(s) which the proof "
            "commits to, or empty array if the proof can not be validated.\n");
    }

    CDataStream ssMB(ParseHexV(request.params[0], "proof"), SER_NETWORK,
                     PROTOCOL_VERSION);
    CMerkleBlock merkleBlock;
    ssMB >> merkleBlock;

    UniValue::Array res;

    std::vector<uint256> vMatch;
    std::vector<size_t> vIndex;
    if (merkleBlock.txn.ExtractMatches(vMatch, vIndex) !=
        merkleBlock.header.hashMerkleRoot) {
        return res;
    }

    LOCK(cs_main);

    const CBlockIndex *pindex = LookupBlockIndex(merkleBlock.header.GetHash());
    if (!pindex || !::ChainActive().Contains(pindex) || pindex->nTx == 0) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           "Block not found in chain");
    }

    // Check if proof is valid, only add results if so
    if (pindex->nTx == merkleBlock.txn.GetNumTransactions()) {
        for (const uint256 &hash : vMatch) {
            res.emplace_back(hash.GetHex());
        }
    }

    return res;
}

CMutableTransaction ConstructTransaction(const CChainParams &params,
                                         const UniValue &inputs_in,
                                         const UniValue &outputs_in,
                                         const UniValue &locktime) {
    if (inputs_in.isNull() || outputs_in.isNull()) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            "Invalid parameter, arguments 1 and 2 must be non-null");
    }

    CMutableTransaction rawTx;

    if (!locktime.isNull()) {
        int64_t nLockTime = locktime.get_int64();
        if (nLockTime < 0 || nLockTime > std::numeric_limits<uint32_t>::max()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                               "Invalid parameter, locktime out of range");
        }

        rawTx.nLockTime = nLockTime;
    }

    for (const UniValue &o : inputs_in.get_array()) {

        TxId txid(ParseHashO(o, "txid"));

        const UniValue &vout_v = o["vout"];
        if (vout_v.isNull()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                               "Invalid parameter, missing vout key");
        }

        if (!vout_v.isNum()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                               "Invalid parameter, vout must be a number");
        }

        int nOutput = vout_v.get_int();
        if (nOutput < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                               "Invalid parameter, vout must be positive");
        }

        uint32_t nSequence =
            (rawTx.nLockTime ? std::numeric_limits<uint32_t>::max() - 1
                             : std::numeric_limits<uint32_t>::max());

        // Set the sequence number if passed in the parameters object.
        const UniValue &sequenceObj = o["sequence"];
        if (sequenceObj.isNum()) {
            int64_t seqNr64 = sequenceObj.get_int64();
            if (seqNr64 < 0 || seqNr64 > std::numeric_limits<uint32_t>::max()) {
                throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    "Invalid parameter, sequence number is out of range");
            }

            nSequence = uint32_t(seqNr64);
        }

        CTxIn in(COutPoint(txid, nOutput), CScript(), nSequence);
        rawTx.vin.push_back(in);
    }

    std::set<CTxDestination> destinations;
    UniValue::Object outputsConverted;
    const UniValue::Object* outputs = &outputsConverted;
    if (outputs_in.isObject()) {
        // Point to the original dict
        outputs = &outputs_in.get_obj();
    } else {
        // Translate array of key-value pairs into dict
        const UniValue::Array& outputsArray = outputs_in.get_array();
        outputsConverted.reserve(outputsArray.size());
        for (const UniValue& output : outputsArray) {
            if (!output.isObject()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                                   "Invalid parameter, key-value pair not an "
                                   "object as expected");
            }
            if (output.size() != 1) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                                   "Invalid parameter, key-value pair must "
                                   "contain exactly one key");
            }
            auto& outputKV = *output.get_obj().begin();
            // Allowing duplicate key insertions here is intentional.
            // Checking for duplicate keys would break functionality, constructing a transaction with missing outputs.
            outputsConverted.emplace_back(outputKV.first, outputKV.second);
        }
    }
    for (auto &entry : *outputs) {
        if (entry.first == "data") {
            std::vector<uint8_t> data = ParseHexV(entry.second, "Data");

            CTxOut out(Amount::zero(), CScript() << OP_RETURN << data);
            rawTx.vout.push_back(out);
        } else {
            CTxDestination destination = DecodeDestination(entry.first, params);
            if (!IsValidDestination(destination)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                                   std::string("Invalid Radiant address: ") +
                                       entry.first);
            }

            if (!destinations.insert(destination).second) {
                throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    std::string("Invalid parameter, duplicated address: ") +
                        entry.first);
            }

            CScript scriptPubKey = GetScriptForDestination(destination);
            Amount nAmount = AmountFromValue(entry.second);

            CTxOut out(nAmount, scriptPubKey);
            rawTx.vout.push_back(out);
        }
    }

    return rawTx;
}

static UniValue createrawtransaction(const Config &config,
                                     const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 2 ||
        request.params.size() > 3) {
        throw std::runtime_error(
            RPCHelpMan{"createrawtransaction",
                "\nCreate a transaction spending the given inputs and creating new outputs.\n"
                "Outputs can be addresses or data.\n"
                "Returns hex-encoded raw transaction.\n"
                "Note that the transaction's inputs are not signed, and\n"
                "it is not stored in the wallet or transmitted to the network.\n",
                {
                    {"inputs", RPCArg::Type::ARR, /* opt */ false, /* default_val */ "", "A json array of json objects",
                        {
                            {"", RPCArg::Type::OBJ, /* opt */ false, /* default_val */ "", "",
                                {
                                    {"txid", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The transaction id"},
                                    {"vout", RPCArg::Type::NUM, /* opt */ false, /* default_val */ "", "The output number"},
                                    {"sequence", RPCArg::Type::NUM, /* opt */ true, /* default_val */ "", "The sequence number"},
                                },
                                },
                        },
                        },
                    {"outputs", RPCArg::Type::ARR, /* opt */ false, /* default_val */ "", "a json array with outputs (key-value pairs).\n"
                            "For compatibility reasons, a dictionary, which holds the key-value pairs directly, is also\n"
                            "                             accepted as second parameter.",
                        {
                            {"", RPCArg::Type::OBJ, /* opt */ true, /* default_val */ "", "",
                                {
                                    {"address", RPCArg::Type::AMOUNT, /* opt */ false, /* default_val */ "", "A key-value pair. The key (string) is the Radiant address, the value (float or string) is the amount in " + CURRENCY_UNIT},
                                },
                                },
                            {"", RPCArg::Type::OBJ, /* opt */ true, /* default_val */ "", "",
                                {
                                    {"data", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "A key-value pair. The key must be \"data\", the value is hex-encoded data"},
                                },
                                },
                        },
                        },
                    {"locktime", RPCArg::Type::NUM, /* opt */ true, /* default_val */ "0", "Raw locktime. Non-0 value also locktime-activates inputs"},
                }}
                .ToString() +
            "\nResult:\n"
            "\"transaction\"              (string) hex string of the transaction\n"

            "\nExamples:\n"
            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"[{\\\"address\\\":0.01}]\"")
            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"[{\\\"data\\\":\\\"00010203\\\"}]\"")
            + HelpExampleRpc("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"[{\\\"address\\\":0.01}]\"")
            + HelpExampleRpc("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"[{\\\"data\\\":\\\"00010203\\\"}]\"")
        );
    }

    RPCTypeCheck(request.params,
                 {UniValue::VARR,
                  UniValue::VARR|UniValue::VOBJ,
                  UniValue::VNUM|UniValue::VNULL});

    CMutableTransaction rawTx =
        ConstructTransaction(config.GetChainParams(), request.params[0],
                             request.params[1], request.params[2]);

    return EncodeHexTx(CTransaction(rawTx));
}

static UniValue decoderawtransaction(const Config &config,
                                     const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"decoderawtransaction",
                "\nReturn a JSON object representing the serialized, hex-encoded transaction.\n",
                {
                    {"hexstring", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The transaction hex string"},
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"txid\" : \"id\",        (string) The transaction id\n"
            "  \"hash\" : \"id\",        (string) The transaction hash\n"
            "  \"size\" : n,             (numeric) The transaction size\n"
            "  \"version\" : n,          (numeric) The version\n"
            "  \"locktime\" : ttt,       (numeric) The lock time\n"
            "  \"vin\" : [               (array of json objects)\n"
            "     {\n"
            "       \"txid\": \"id\",    (string) The transaction id\n"
            "       \"vout\": n,         (numeric) The output number\n"
            "       \"scriptSig\": {     (json object) The script\n"
            "         \"asm\": \"asm\",  (string) asm\n"
            "         \"hex\": \"hex\"   (string) hex\n"
            "       },\n"
            "       \"sequence\": n     (numeric) The script sequence number\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"vout\" : [             (array of json objects)\n"
            "     {\n"
            "       \"value\" : x.xxx,            (numeric) The value in " +
            CURRENCY_UNIT +
            "\n"
            "       \"n\" : n,                    (numeric) index\n"
            "       \"scriptPubKey\" : {          (json object)\n"
            "         \"asm\" : \"asm\",          (string) the asm\n"
            "         \"hex\" : \"hex\",          (string) the hex\n"
            "         \"reqSigs\" : n,            (numeric) The required sigs\n"
            "         \"type\" : \"pubkeyhash\",  (string) The type, eg "
            "'pubkeyhash'\n"
            "         \"addresses\" : [           (json array of string)\n"
            "           \"12tvKAXCxZjSmdNbao16dKXC8tRWfcF5oc\"   (string) "
            "Radiant address\n"
            "           ,...\n"
            "         ]\n"
            "       }\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("decoderawtransaction", "\"hexstring\"") +
            HelpExampleRpc("decoderawtransaction", "\"hexstring\""));
    }

    RPCTypeCheck(request.params, {UniValue::VSTR});

    CMutableTransaction mtx;

    if (!DecodeHexTx(mtx, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    return TxToUniv(config, CTransaction(std::move(mtx)), uint256(), false);
}

static UniValue decodescript(const Config &config,
                             const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"decodescript",
                "\nDecode a hex-encoded script.\n",
                {
                    {"hexstring", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "the hex-encoded script"},
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"asm\":\"asm\",   (string) Script public key\n"
            "  \"type\":\"type\", (string) The output type\n"
            "  \"reqSigs\": n,    (numeric) The required signatures\n"
            "  \"addresses\": [   (json array of string)\n"
            "     \"address\"     (string) Radiant address\n"
            "     ,...\n"
            "  ],\n"
            "  \"p2sh\",\"address\" (string) address of P2SH script wrapping "
            "this redeem script (not returned if the script is already a "
            "P2SH).\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("decodescript", "\"hexstring\"") +
            HelpExampleRpc("decodescript", "\"hexstring\""));
    }

    RPCTypeCheck(request.params, {UniValue::VSTR});

    CScript script;
    if (!request.params[0].get_str().empty()) {
        std::vector<uint8_t> scriptData(
            ParseHexV(request.params[0], "argument"));
        script = CScript(scriptData.begin(), scriptData.end());
    } else {
        // Empty scripts are valid.
    }

    return ScriptPubKeyToUniv(config, script, false, true);
}

/**
 * Returns a JSON object for script verification or signing errors.
 */
static UniValue::Object TxInErrorToJSON(const CTxIn& txin, std::string&& strMessage) {
    UniValue::Object entry;
    entry.reserve(5);
    entry.emplace_back("txid", txin.prevout.GetTxId().ToString());
    entry.emplace_back("vout", txin.prevout.GetN());
    entry.emplace_back("scriptSig", HexStr(txin.scriptSig));
    entry.emplace_back("sequence", txin.nSequence);
    entry.emplace_back("error", std::move(strMessage));
    return entry;
}

static UniValue combinerawtransaction(const Config &,
                                      const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"combinerawtransaction",
                "\nCombine multiple partially signed transactions into one transaction.\n"
                "The combined transaction may be another partially signed transaction or a\n"
                "fully signed transaction.",
                {
                    {"txs", RPCArg::Type::ARR, /* opt */ false, /* default_val */ "", "A json array of hex strings of partially signed transactions",
                        {
                            {"hexstring", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "A transaction hash"},
                        },
                        },
                }}
                .ToString() +
            "\nResult:\n"
            "\"hex\"            (string) The hex-encoded raw transaction with "
            "signature(s)\n"

            "\nExamples:\n" +
            HelpExampleCli("combinerawtransaction",
                           "[\"myhex1\", \"myhex2\", \"myhex3\"]"));
    }

    const UniValue::Array& txs = request.params[0].get_array();
    std::vector<CMutableTransaction> txVariants(txs.size());

    for (unsigned int idx = 0; idx < txs.size(); idx++) {
        if (!DecodeHexTx(txVariants[idx], txs[idx].get_str())) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                               strprintf("TX decode failed for tx %d", idx));
        }
    }

    if (txVariants.empty()) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Missing transactions");
    }

    // mergedTx will end up with all the signatures; it
    // starts as a clone of the rawtx:
    CMutableTransaction mergedTx(txVariants[0]);

    // Fetch previous transactions (inputs):
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(cs_main);
        LOCK(g_mempool.cs);
        CCoinsViewCache &viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, g_mempool);
        // temporarily switch cache backend to db+mempool view
        view.SetBackend(viewMempool);

        for (const CTxIn &txin : mergedTx.vin) {
            // Load entries from viewChain into view; can fail.
            view.AccessCoin(txin.prevout);
        }

        // pre-load all the coins for the other txns too (for context info below)
        for (size_t i = 1; i < txVariants.size(); ++i) {
            const auto &txv = txVariants[i];
            for (const CTxIn &txin : txv.vin) {
                // Load entries from viewChain into view; can fail.
                view.AccessCoin(txin.prevout);
            }
        }

        // switch back to avoid locking mempool for too long
        view.SetBackend(viewDummy);
    }

    // Assumption: Below code does NOT push_back new inputs to mergedTx.
    const auto contexts = ScriptExecutionContext::createForAllInputs(mergedTx, view);
    assert(contexts.size() == mergedTx.vin.size());

    // Sign what we can:
    for (size_t i = 0; i < mergedTx.vin.size(); i++) {
        CTxIn &txin = mergedTx.vin[i];
        const Coin &coin = contexts[i].coin(i); // this coin came from "view" above
        if (coin.IsSpent()) {
            throw JSONRPCError(RPC_VERIFY_ERROR,
                               "Input not found or already spent");
        }
        SignatureData sigdata;

        const CTxOut &txout = coin.GetTxOut();

        // ... and merge in other signatures:
        for (const CMutableTransaction &txv : txVariants) {
            if (txv.vin.size() > i) {
                const auto txvContexts = ScriptExecutionContext::createForAllInputs(txv, view);
                sigdata.MergeSignatureData(DataFromTransaction(txv, i, txout, txvContexts[i]));
            }
        }

        ProduceSignature(
            DUMMY_SIGNING_PROVIDER,
            MutableTransactionSignatureCreator(&mergedTx, i, txout.nValue),
            txout.scriptPubKey, sigdata, contexts[i]);

        UpdateInput(txin, sigdata);
    }

    return EncodeHexTx(CTransaction(mergedTx));
}

UniValue::Object SignTransaction(interfaces::Chain &, CMutableTransaction &mtx, const UniValue &prevTxsUnival,
                                 CBasicKeyStore *keystore, bool is_temp_keystore, const UniValue &hashType) {
    // Fetch previous transactions (inputs):
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK2(cs_main, g_mempool.cs);
        CCoinsViewCache &viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, g_mempool);
        // Temporarily switch cache backend to db+mempool view.
        view.SetBackend(viewMempool);

        for (const CTxIn &txin : mtx.vin) {
            // Load entries from viewChain into view; can fail.
            view.AccessCoin(txin.prevout);
        }

        // Switch back to avoid locking mempool for too long.
        view.SetBackend(viewDummy);
    }

    // Add previous txouts given in the RPC call:
    if (!prevTxsUnival.isNull()) {
        for (const UniValue& p : prevTxsUnival.get_array()) {
            if (!p.isObject()) {
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                                   "expected object with "
                                   "{\"txid'\",\"vout\",\"scriptPubKey\"}");
            }

            const UniValue::Object& prevOut = p.get_obj();

            RPCTypeCheckObj(prevOut,
                            {
                                {"txid", UniValue::VSTR},
                                {"vout", UniValue::VNUM},
                                {"scriptPubKey", UniValue::VSTR},
                                {"amount", UniValue::VNUM|UniValue::VSTR},
                            });

            TxId txid(ParseHashO(prevOut, "txid"));

            int nOut = prevOut["vout"].get_int();
            if (nOut < 0) {
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                                   "vout must be positive");
            }

            COutPoint out(txid, nOut);
            std::vector<uint8_t> pkData(ParseHexO(prevOut, "scriptPubKey"));
            CScript scriptPubKey(pkData.begin(), pkData.end());

            {
                const Coin &coin = view.AccessCoin(out);
                if (!coin.IsSpent() &&
                    coin.GetTxOut().scriptPubKey != scriptPubKey) {
                    std::string err("Previous output scriptPubKey mismatch:\n");
                    err = err + ScriptToAsmStr(coin.GetTxOut().scriptPubKey) +
                          "\nvs:\n" + ScriptToAsmStr(scriptPubKey);
                    throw JSONRPCError(RPC_DESERIALIZATION_ERROR, std::move(err));
                }

                CTxOut txout;
                txout.scriptPubKey = scriptPubKey;
                txout.nValue = AmountFromValue(prevOut["amount"]);

                view.AddCoin(out, Coin(txout, 1, false), true);
            }

            // If redeemScript and private keys were given, add redeemScript to
            // the keystore so it can be signed
            if (is_temp_keystore && scriptPubKey.IsPayToScriptHash()) {
                RPCTypeCheckObj(prevOut,
                                {
                                    {"redeemScript", UniValue::VSTR},
                                });
                std::vector<uint8_t> rsData(ParseHexO(prevOut, "redeemScript"));
                CScript redeemScript(rsData.begin(), rsData.end());
                keystore->AddCScript(redeemScript);
            }
        }
    }

    SigHashType sigHashType = ParseSighashString(hashType);
    if (!sigHashType.hasForkId()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Signature must use SIGHASH_FORKID");
    }

    // Script verification errors.
    UniValue::Array vErrors;

    // Use CTransaction for the constant parts of the transaction to avoid
    // rehashing.
    const CTransaction txConst(mtx);
    // Assumption: Below code does NOT push_back new inputs to mtx.
    const auto contexts = ScriptExecutionContext::createForAllInputs(mtx, view);
    // Sign what we can:
    for (size_t i = 0; i < mtx.vin.size(); i++) {
        CTxIn &txin = mtx.vin[i];
        const Coin &coin = contexts[i].coin(i); // this coin ultimately comes from "view" above
        if (coin.IsSpent()) {
            vErrors.emplace_back(TxInErrorToJSON(txin, "Input not found or already spent"));
            continue;
        }

        const CScript &prevPubKey = coin.GetTxOut().scriptPubKey;
        const Amount amount = coin.GetTxOut().nValue;

        SignatureData sigdata = DataFromTransaction(mtx, i, coin.GetTxOut(), contexts[i]);

        // Only sign SIGHASH_SINGLE if there's a corresponding output:
        if ((sigHashType.getBaseType() != BaseSigHashType::SINGLE) ||
            (i < mtx.vout.size())) {
            ProduceSignature(*keystore,
                             MutableTransactionSignatureCreator(&mtx, i, amount, sigHashType),
                             prevPubKey, sigdata, contexts[i]);
        }

        UpdateInput(txin, sigdata);

        ScriptError serror = ScriptError::OK;
        if ( ! VerifyScript(txin.scriptSig, prevPubKey, STANDARD_SCRIPT_VERIFY_FLAGS, TransactionSignatureChecker(&txConst, i, amount), contexts[i], &serror)) {
            if (serror == ScriptError::INVALID_STACK_OPERATION) {
                // Unable to sign input and verification failed (possible
                // attempt to partially sign).
                vErrors.emplace_back(TxInErrorToJSON(txin, "Unable to sign input, invalid stack size (possibly missing key)"));
            } else {
                vErrors.emplace_back(TxInErrorToJSON(txin, ScriptErrorString(serror)));
            }
        }
    }

    bool fComplete = vErrors.empty();

    UniValue::Object result;
    result.reserve(fComplete ? 2 : 3);
    result.emplace_back("hex", EncodeHexTx(CTransaction(mtx)));
    result.emplace_back("complete", fComplete);
    if (!fComplete) {
        result.emplace_back("errors", std::move(vErrors));
    }

    return result;
}

static UniValue signrawtransactionwithkey(const Config &,
                                          const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 2 ||
        request.params.size() > 4) {
        throw std::runtime_error(
            RPCHelpMan{"signrawtransactionwithkey",
                "\nSign inputs for raw transaction (serialized, hex-encoded).\n"
                "The second argument is an array of base58-encoded private\n"
                "keys that will be the only keys used to sign the transaction.\n"
                "The third optional argument (may be null) is an array of previous transaction outputs that\n"
                "this transaction depends on but may not yet be in the block chain.\n",
                {
                    {"hexstring", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "The transaction hex string"},
                    {"privkeys", RPCArg::Type::ARR, /* opt */ false, /* default_val */ "", "A json array of base58-encoded private keys for signing",
                        {
                            {"privatekey", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "private key in base58-encoding"},
                        },
                        },
                    {"prevtxs", RPCArg::Type::ARR, /* opt */ true, /* default_val */ "", "A json array of previous dependent transaction outputs",
                        {
                            {"", RPCArg::Type::OBJ, /* opt */ true, /* default_val */ "", "",
                                {
                                    {"txid", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The transaction id"},
                                    {"vout", RPCArg::Type::NUM, /* opt */ false, /* default_val */ "", "The output number"},
                                    {"scriptPubKey", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "script key"},
                                    {"redeemScript", RPCArg::Type::STR_HEX, /* opt */ true, /* default_val */ "", "(required for P2SH or P2WSH) redeem script"},
                                    {"amount", RPCArg::Type::AMOUNT, /* opt */ false, /* default_val */ "", "The amount spent"},
                                },
                                },
                        },
                        },
                    {"sighashtype", RPCArg::Type::STR, /* opt */ true, /* default_val */ "ALL", "The signature hash type. Must be one of:\n"
            "       \"ALL\"\n"
            "       \"NONE\"\n"
            "       \"SINGLE\"\n"
            "       \"ALL|ANYONECANPAY\"\n"
            "       \"NONE|ANYONECANPAY\"\n"
            "       \"SINGLE|ANYONECANPAY\"\n"
                    },
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"hex\" : \"value\",                  (string) The hex-encoded "
            "raw transaction with signature(s)\n"
            "  \"complete\" : true|false,          (boolean) If the "
            "transaction has a complete set of signatures\n"
            "  \"errors\" : [                      (json array of objects) "
            "Script verification errors (if there are any)\n"
            "    {\n"
            "      \"txid\" : \"hash\",              (string) The hash of the "
            "referenced, previous transaction\n"
            "      \"vout\" : n,                   (numeric) The index of the "
            "output to spent and used as input\n"
            "      \"scriptSig\" : \"hex\",          (string) The hex-encoded "
            "signature script\n"
            "      \"sequence\" : n,               (numeric) Script sequence "
            "number\n"
            "      \"error\" : \"text\"              (string) Verification or "
            "signing error related to the input\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("signrawtransactionwithkey", "\"myhex\"") +
            HelpExampleRpc("signrawtransactionwithkey", "\"myhex\""));
    }

    RPCTypeCheck(
        request.params,
        {UniValue::VSTR, UniValue::VARR, UniValue::VARR|UniValue::VNULL, UniValue::VSTR|UniValue::VNULL});

    CMutableTransaction mtx;
    if (!DecodeHexTx(mtx, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    CBasicKeyStore keystore;
    for (const UniValue &k : request.params[1].get_array()) {
        CKey key = DecodeSecret(k.get_str());
        if (!key.IsValid()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                               "Invalid private key");
        }
        keystore.AddKey(key);
    }

    return SignTransaction(*g_rpc_node->chain, mtx, request.params[2],
                           &keystore, true, request.params[3]);
}

static UniValue sendrawtransaction(const Config &config,
                                   const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"sendrawtransaction",
                "\nSubmits raw transaction (serialized, hex-encoded) to local node and network.\n"
                "\nAlso see createrawtransaction and signrawtransactionwithkey calls.\n",
                {
                    {"hexstring", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The hex string of the raw transaction"},
                    {"allowhighfees", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "false", "Allow high fees"},
                }}
                .ToString() +
            "\nResult:\n"
            "\"hex\"             (string) The transaction hash in hex\n"
            "\nExamples:\n"
            "\nCreate a transaction\n" +
            HelpExampleCli(
                "createrawtransaction",
                "\"[{\\\"txid\\\" : \\\"mytxid\\\",\\\"vout\\\":0}]\" "
                "\"{\\\"myaddress\\\":0.01}\"") +
            "Sign the transaction, and get back the hex\n" +
            HelpExampleCli("signrawtransactionwithwallet", "\"myhex\"") +
            "\nSend the transaction (signed hex)\n" +
            HelpExampleCli("sendrawtransaction", "\"signedhex\"") +
            "\nAs a JSON-RPC call\n" +
            HelpExampleRpc("sendrawtransaction", "\"signedhex\""));
    }

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::MBOOL});

    // parse hex string from parameter
    CMutableTransaction mtx;
    if (!DecodeHexTx(mtx, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    CTransactionRef tx(MakeTransactionRef(std::move(mtx)));

    bool allowhighfees = false;
    if (!request.params[1].isNull()) {
        allowhighfees = request.params[1].get_bool();
    }

    return BroadcastTransaction(config, tx, allowhighfees).GetHex();
}

static UniValue testmempoolaccept(const Config &config,
                                  const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"testmempoolaccept",
                "\nReturns result of mempool acceptance tests indicating if raw transaction (serialized, hex-encoded) would be accepted by mempool.\n"
                "\nThis checks if the transaction violates the consensus or policy rules.\n"
                "\nSee sendrawtransaction call.\n",
                {
                    {"rawtxs", RPCArg::Type::ARR, /* opt */ false, /* default_val */ "", "An array of hex strings of raw transactions.\n"
            "                                        Length must be one for now.",
                        {
                            {"rawtx", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", ""},
                        },
                        },
                    {"allowhighfees", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "false", "Allow high fees"},
                }}
                .ToString() +
            "\nResult:\n"
            "[                   (array) The result of the mempool acceptance test for each raw transaction in the input array.\n"
            "                            Length is exactly one for now.\n"
            " {\n"
            "  \"txid\"           (string) The transaction hash in hex\n"
            "  \"allowed\"        (boolean) If the mempool allows this tx to be inserted\n"
            "  \"reject-reason\"  (string) Rejection string (only present when 'allowed' is false)\n"
            " }\n"
            "]\n"
            "\nExamples:\n"
            "\nCreate a transaction\n"
            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\" : \\\"mytxid\\\",\\\"vout\\\":0}]\" \"{\\\"myaddress\\\":0.01}\"") +
            "Sign the transaction, and get back the hex\n"
            + HelpExampleCli("signrawtransactionwithwallet", "\"myhex\"") +
            "\nTest acceptance of the transaction (signed hex)\n"
            + HelpExampleCli("testmempoolaccept", "\"signedhex\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("testmempoolaccept", "[\"signedhex\"]")
        );
    }

    RPCTypeCheck(request.params, {UniValue::VARR, UniValue::MBOOL});
    if (request.params[0].get_array().size() != 1) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            "Array must contain exactly one raw transaction for now");
    }

    CMutableTransaction mtx;
    if (!DecodeHexTx(mtx, request.params[0].get_array()[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }
    CTransactionRef tx(MakeTransactionRef(std::move(mtx)));
    const uint256 &txid = tx->GetId();

    Amount max_raw_tx_fee = maxTxFee;
    if (!request.params[1].isNull() && request.params[1].get_bool()) {
        max_raw_tx_fee = Amount::zero();
    }

    CValidationState state;
    bool missing_inputs;
    bool test_accept_res;
    {
        LOCK(cs_main);
        test_accept_res = AcceptToMemoryPool(
            config, g_mempool, state, std::move(tx), &missing_inputs,
            false /* bypass_limits */, max_raw_tx_fee, true /* test_accept */);
    }

    UniValue::Array result;
    result.reserve(1);
    UniValue::Object result_0;
    result_0.reserve(test_accept_res ? 2 : 3);
    result_0.emplace_back("txid", txid.GetHex());
    result_0.emplace_back("allowed", test_accept_res);
    if (!test_accept_res) {
        if (state.IsInvalid()) {
            result_0.emplace_back("reject-reason", strprintf("%i: %s", state.GetRejectCode(), state.GetRejectReason()));
        } else if (missing_inputs) {
            result_0.emplace_back("reject-reason", "missing-inputs");
        } else {
            result_0.emplace_back("reject-reason", state.GetRejectReason());
        }
    }
    result.emplace_back(std::move(result_0));
    return result;
}

static std::string WriteHDKeypath(const std::vector<uint32_t> &keypath) {
    std::string keypath_str = "m";
    for (uint32_t num : keypath) {
        keypath_str += "/";
        bool hardened = false;
        if (num & 0x80000000) {
            hardened = true;
            num &= ~0x80000000;
        }

        keypath_str += std::to_string(num);
        if (hardened) {
            keypath_str += "'";
        }
    }
    return keypath_str;
}

static UniValue decodepsbt(const Config &config,
                           const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"decodepsbt",
                "\nReturn a JSON object representing the serialized, base64-encoded partially signed Bitcoin transaction.\n",
                {
                    {"psbt", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "The PSBT base64 string"},
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"tx\" : {                   (json object) The decoded "
            "network-serialized unsigned transaction.\n"
            "    ...                                      The layout is the "
            "same as the output of decoderawtransaction.\n"
            "  },\n"
            "  \"unknown\" : {                (json object) The unknown global "
            "fields\n"
            "    \"key\" : \"value\"            (key-value pair) An unknown "
            "key-value pair\n"
            "     ...\n"
            "  },\n"
            "  \"inputs\" : [                 (array of json objects)\n"
            "    {\n"
            "      \"utxo\" : {            (json object, optional) Transaction "
            "output for UTXOs\n"
            "        \"amount\" : x.xxx,           (numeric) The value in " +
            CURRENCY_UNIT +
            "\n"
            "        \"scriptPubKey\" : {          (json object)\n"
            "          \"asm\" : \"asm\",            (string) The asm\n"
            "          \"hex\" : \"hex\",            (string) The hex\n"
            "          \"type\" : \"pubkeyhash\",    (string) The type, eg "
            "'pubkeyhash'\n"
            "          \"address\" : \"address\"     (string) Radiant address "
            "if there is one\n"
            "        }\n"
            "      },\n"
            "      \"partial_signatures\" : {             (json object, "
            "optional)\n"
            "        \"pubkey\" : \"signature\",           (string) The public "
            "key and signature that corresponds to it.\n"
            "        ,...\n"
            "      }\n"
            "      \"sighash\" : \"type\",                  (string, optional) "
            "The sighash type to be used\n"
            "      \"redeem_script\" : {       (json object, optional)\n"
            "          \"asm\" : \"asm\",            (string) The asm\n"
            "          \"hex\" : \"hex\",            (string) The hex\n"
            "          \"type\" : \"pubkeyhash\",    (string) The type, eg "
            "'pubkeyhash'\n"
            "        }\n"
            "      \"bip32_derivs\" : {          (json object, optional)\n"
            "        \"pubkey\" : {                     (json object, "
            "optional) The public key with the derivation path as the value.\n"
            "          \"master_fingerprint\" : \"fingerprint\"     (string) "
            "The fingerprint of the master key\n"
            "          \"path\" : \"path\",                         (string) "
            "The path\n"
            "        }\n"
            "        ,...\n"
            "      }\n"
            "      \"final_scriptsig\" : {       (json object, optional)\n"
            "          \"asm\" : \"asm\",            (string) The asm\n"
            "          \"hex\" : \"hex\",            (string) The hex\n"
            "        }\n"
            "      \"unknown\" : {                (json object) The unknown "
            "global fields\n"
            "        \"key\" : \"value\"            (key-value pair) An "
            "unknown key-value pair\n"
            "         ...\n"
            "      },\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "  \"outputs\" : [                 (array of json objects)\n"
            "    {\n"
            "      \"redeem_script\" : {       (json object, optional)\n"
            "          \"asm\" : \"asm\",            (string) The asm\n"
            "          \"hex\" : \"hex\",            (string) The hex\n"
            "          \"type\" : \"pubkeyhash\",    (string) The type, eg "
            "'pubkeyhash'\n"
            "        }\n"
            "      \"bip32_derivs\" : [          (array of json objects, "
            "optional)\n"
            "        {\n"
            "          \"pubkey\" : \"pubkey\",                     (string) "
            "The public key this path corresponds to\n"
            "          \"master_fingerprint\" : \"fingerprint\"     (string) "
            "The fingerprint of the master key\n"
            "          \"path\" : \"path\",                         (string) "
            "The path\n"
            "          }\n"
            "        }\n"
            "        ,...\n"
            "      ],\n"
            "      \"unknown\" : {                (json object) The unknown "
            "global fields\n"
            "        \"key\" : \"value\"            (key-value pair) An "
            "unknown key-value pair\n"
            "         ...\n"
            "      },\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "  \"fee\" : fee                      (numeric, optional) The "
            "transaction fee paid if all UTXOs slots in the PSBT have been "
            "filled.\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("decodepsbt", "\"psbt\""));
    }

    RPCTypeCheck(request.params, {UniValue::VSTR});

    // Unserialize the transactions
    PartiallySignedTransaction psbtx;
    std::string error;
    if (!DecodePSBT(psbtx, request.params[0].get_str(), error)) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                           strprintf("TX decode failed %s", error));
    }

    UniValue::Object result;

    // Add the decoded tx
    result.emplace_back("tx", TxToUniv(config, CTransaction(*psbtx.tx), uint256(), false));

    // Unknown data
    if (!psbtx.unknown.empty()) {
        UniValue::Object unknowns;
        unknowns.reserve(psbtx.unknown.size());
        for (const auto &entry : psbtx.unknown) {
            unknowns.emplace_back(HexStr(entry.first), HexStr(entry.second));
        }
        result.emplace_back("unknown", std::move(unknowns));
    }

    // inputs
    Amount total_in = Amount::zero();
    bool have_all_utxos = true;
    UniValue::Array inputs;
    inputs.reserve(psbtx.inputs.size());
    for (size_t i = 0; i < psbtx.inputs.size(); ++i) {
        const PSBTInput &input = psbtx.inputs[i];
        UniValue::Object in;
        // UTXOs
        if (!input.utxo.IsNull()) {
            const CTxOut &txout = input.utxo;

            UniValue::Object out;
            out.reserve(2);

            out.emplace_back("amount", ValueFromAmount(txout.nValue));
            total_in += txout.nValue;

            out.emplace_back("scriptPubKey", ScriptToUniv(config, txout.scriptPubKey, true));
            in.emplace_back("utxo", std::move(out));
        } else {
            have_all_utxos = false;
        }

        // Partial sigs
        if (!input.partial_sigs.empty()) {
            UniValue::Object partial_sigs;
            for (const auto &sig : input.partial_sigs) {
                partial_sigs.emplace_back(HexStr(sig.second.first), HexStr(sig.second.second));
            }
            in.emplace_back("partial_signatures", std::move(partial_sigs));
        }

        // Sighash
        uint8_t sighashbyte = input.sighash_type.getRawSigHashType() & 0xff;
        if (sighashbyte > 0) {
            in.emplace_back("sighash", SighashToStr(sighashbyte));
        }

        // Redeem script
        if (!input.redeem_script.empty()) {
            in.emplace_back("redeem_script", ScriptToUniv(config, input.redeem_script, false));
        }

        // keypaths
        if (!input.hd_keypaths.empty()) {
            UniValue::Array keypaths;
            keypaths.reserve(input.hd_keypaths.size());
            for (const auto &entry : input.hd_keypaths) {
                UniValue::Object keypath;
                keypath.reserve(3);
                keypath.emplace_back("pubkey", HexStr(entry.first));
                keypath.emplace_back("master_fingerprint", strprintf("%08x", ReadBE32(entry.second.fingerprint)));
                keypath.emplace_back("path", WriteHDKeypath(entry.second.path));
                keypaths.emplace_back(std::move(keypath));
            }
            in.emplace_back("bip32_derivs", std::move(keypaths));
        }

        // Final scriptSig
        if (!input.final_script_sig.empty()) {
            UniValue::Object scriptsig;
            scriptsig.reserve(2);
            scriptsig.emplace_back("asm", ScriptToAsmStr(input.final_script_sig, true));
            scriptsig.emplace_back("hex", HexStr(input.final_script_sig));
            in.emplace_back("final_scriptSig", std::move(scriptsig));
        }

        // Unknown data
        if (!input.unknown.empty()) {
            UniValue::Object unknowns;
            unknowns.reserve(input.unknown.size());
            for (const auto &entry : input.unknown) {
                unknowns.emplace_back(HexStr(entry.first), HexStr(entry.second));
            }
            in.emplace_back("unknown", std::move(unknowns));
        }

        inputs.emplace_back(std::move(in));
    }
    result.emplace_back("inputs", std::move(inputs));

    // outputs
    Amount output_value = Amount::zero();
    UniValue::Array outputs;
    outputs.reserve(psbtx.outputs.size());
    for (size_t i = 0; i < psbtx.outputs.size(); ++i) {
        const PSBTOutput &output = psbtx.outputs[i];
        UniValue::Object out;
        // Redeem script
        if (!output.redeem_script.empty()) {
            out.emplace_back("redeem_script", ScriptToUniv(config, output.redeem_script, false));
        }

        // keypaths
        if (!output.hd_keypaths.empty()) {
            UniValue::Array keypaths;
            keypaths.reserve(output.hd_keypaths.size());
            for (const auto &entry : output.hd_keypaths) {
                UniValue::Object keypath;
                keypath.reserve(3);
                keypath.emplace_back("pubkey", HexStr(entry.first));
                keypath.emplace_back("master_fingerprint", strprintf("%08x", ReadBE32(entry.second.fingerprint)));
                keypath.emplace_back("path", WriteHDKeypath(entry.second.path));
                keypaths.emplace_back(std::move(keypath));
            }
            out.emplace_back("bip32_derivs", std::move(keypaths));
        }

        // Unknown data
        if (!output.unknown.empty()) {
            UniValue::Object unknowns;
            unknowns.reserve(output.unknown.size());
            for (const auto &entry : output.unknown) {
                unknowns.emplace_back(HexStr(entry.first), HexStr(entry.second));
            }
            out.emplace_back("unknown", std::move(unknowns));
        }

        outputs.emplace_back(std::move(out));

        // Fee calculation
        output_value += psbtx.tx->vout[i].nValue;
    }
    result.emplace_back("outputs", std::move(outputs));
    if (have_all_utxos) {
        result.emplace_back("fee", ValueFromAmount(total_in - output_value));
    }

    return result;
}

static UniValue combinepsbt(const Config &,
                            const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"combinepsbt",
                "\nCombine multiple partially signed Bitcoin transactions into one transaction.\n"
                "Implements the Combiner role.\n",
                {
                    {"txs", RPCArg::Type::ARR, /* opt */ false, /* default_val */ "", "A json array of base64 strings of partially signed transactions",
                        {
                            {"psbt", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "A base64 string of a PSBT"},
                        },
                        },
                }}
                .ToString() +
            "\nResult:\n"
            "  \"psbt\"          (string) The base64-encoded partially signed "
            "transaction\n"
            "\nExamples:\n" +
            HelpExampleCli("combinepsbt",
                           "[\"mybase64_1\", \"mybase64_2\", \"mybase64_3\"]"));
    }

    RPCTypeCheck(request.params, {UniValue::VARR});

    // Unserialize the transactions
    std::vector<PartiallySignedTransaction> psbtxs;
    const UniValue::Array &txs = request.params[0].get_array();
    if (txs.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Parameter 'txs' cannot be empty");
    }
    psbtxs.reserve(txs.size());
    for (const UniValue& tx : txs) {
        PartiallySignedTransaction psbtx;
        std::string error;
        if (!DecodePSBT(psbtx, tx.get_str(), error)) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                               strprintf("TX decode failed %s", error));
        }
        psbtxs.push_back(std::move(psbtx));
    }

    // Copy the first one
    PartiallySignedTransaction merged_psbt(psbtxs[0]);

    // Merge
    for (auto it = std::next(psbtxs.begin()); it != psbtxs.end(); ++it) {
        if (!merged_psbt.Merge(*it)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                               "PSBTs do not refer to the same transactions.");
        }
    }
    if (!merged_psbt.IsSane()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Merged PSBT is inconsistent");
    }

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << merged_psbt;
    return EncodeBase64(MakeUInt8Span(ssTx));
}

static UniValue finalizepsbt(const Config &,
                             const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"finalizepsbt",
                "Finalize the inputs of a PSBT. If the transaction is fully signed, it will produce a\n"
                "network serialized transaction which can be broadcast with sendrawtransaction. Otherwise a PSBT will be\n"
                "created which has the final_scriptSigfields filled for inputs that are complete.\n"
                "Implements the Finalizer and Extractor roles.\n",
                {
                    {"psbt", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "A base64 string of a PSBT"},
                    {"extract", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "true", "If true and the transaction is complete,\n"
            "                             extract and return the complete transaction in normal network serialization instead of the PSBT."},
                }}
                .ToString() +
            "\nResult:\n"
            "{\n"
            "  \"psbt\" : \"value\",          (string) The base64-encoded "
            "partially signed transaction if not extracted\n"
            "  \"hex\" : \"value\",           (string) The hex-encoded network "
            "transaction if extracted\n"
            "  \"complete\" : true|false,   (boolean) If the transaction has a "
            "complete set of signatures\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("finalizepsbt", "\"psbt\""));
    }

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::MBOOL|UniValue::VNULL});

    // Unserialize the transactions
    PartiallySignedTransaction psbtx;
    std::string error;
    if (!DecodePSBT(psbtx, request.params[0].get_str(), error)) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                           strprintf("TX decode failed %s", error));
    }

    // Finalize input signatures -- in case we have partial signatures that add
    // up to a complete
    //   signature, but have not combined them yet (e.g. because the combiner
    //   that created this PartiallySignedTransaction did not understand them),
    //   this will combine them into a final script.
    bool complete = true;
    // Assumption: Below code does NOT push_back new inputs to psbtx.tx.
    const auto contexts = ScriptExecutionContext::createForAllInputs(*psbtx.tx, psbtx.inputs);
    for (size_t i = 0; i < psbtx.tx->vin.size(); ++i) {
        complete &=
            SignPSBTInput(DUMMY_SIGNING_PROVIDER, psbtx, i, SigHashType(), contexts[i]);
    }

    UniValue::Object result;
    result.reserve(2);
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    bool extract = request.params[1].isNull() || request.params[1].get_bool();
    if (complete && extract) {
        CMutableTransaction mtx(*psbtx.tx);
        for (size_t i = 0; i < mtx.vin.size(); ++i) {
            mtx.vin[i].scriptSig = psbtx.inputs[i].final_script_sig;
        }
        ssTx << mtx;
        result.emplace_back("hex", HexStr(ssTx));
    } else {
        ssTx << psbtx;
        result.emplace_back("psbt", EncodeBase64(MakeUInt8Span(ssTx)));
    }
    result.emplace_back("complete", complete);

    return result;
}

static UniValue createpsbt(const Config &config,
                           const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 2 ||
        request.params.size() > 3) {
        throw std::runtime_error(
            RPCHelpMan{"createpsbt",
                "\nCreates a transaction in the Partially Signed Transaction format.\n"
                "Implements the Creator role.\n",
                {
                    {"inputs", RPCArg::Type::ARR, /* opt */ false, /* default_val */ "", "A json array of json objects",
                        {
                            {"", RPCArg::Type::OBJ, /* opt */ false, /* default_val */ "", "",
                                {
                                    {"txid", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The transaction id"},
                                    {"vout", RPCArg::Type::NUM, /* opt */ false, /* default_val */ "", "The output number"},
                                    {"sequence", RPCArg::Type::NUM, /* opt */ true, /* default_val */ "", "The sequence number"},
                                },
                                },
                        },
                        },
                    {"outputs", RPCArg::Type::ARR, /* opt */ false, /* default_val */ "", "a json array with outputs (key-value pairs).\n"
                            "For compatibility reasons, a dictionary, which holds the key-value pairs directly, is also\n"
                            "                             accepted as second parameter.",
                        {
                            {"", RPCArg::Type::OBJ, /* opt */ true, /* default_val */ "", "",
                                {
                                    {"address", RPCArg::Type::AMOUNT, /* opt */ false, /* default_val */ "", "A key-value pair. The key (string) is the Radiant address, the value (float or string) is the amount in " + CURRENCY_UNIT},
                                },
                                },
                            {"", RPCArg::Type::OBJ, /* opt */ true, /* default_val */ "", "",
                                {
                                    {"data", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "A key-value pair. The key must be \"data\", the value is hex-encoded data"},
                                },
                                },
                        },
                        },
                    {"locktime", RPCArg::Type::NUM, /* opt */ true, /* default_val */ "0", "Raw locktime. Non-0 value also locktime-activates inputs"},
                }}
                .ToString() +
                            "\nResult:\n"
                            "  \"psbt\"        (string)  The resulting raw transaction (base64-encoded string)\n"
                            "\nExamples:\n"
                            + HelpExampleCli("createpsbt", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"[{\\\"data\\\":\\\"00010203\\\"}]\"")
                            );
    }


    RPCTypeCheck(request.params,
                 {
                     UniValue::VARR,
                     UniValue::VARR|UniValue::VOBJ,
                     UniValue::VNUM|UniValue::VNULL,
                 });

    CMutableTransaction rawTx =
        ConstructTransaction(config.GetChainParams(), request.params[0],
                             request.params[1], request.params[2]);

    // Make a blank psbt
    PartiallySignedTransaction psbtx;
    psbtx.tx = rawTx;
    psbtx.inputs.reserve(rawTx.vin.size());
    for (size_t i = 0; i < rawTx.vin.size(); ++i) {
        psbtx.inputs.push_back(PSBTInput());
    }
    psbtx.outputs.reserve(rawTx.vout.size());
    for (size_t i = 0; i < rawTx.vout.size(); ++i) {
        psbtx.outputs.push_back(PSBTOutput());
    }

    // Serialize the PSBT
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << psbtx;

    return EncodeBase64(MakeUInt8Span(ssTx));
}

static UniValue converttopsbt(const Config &,
                              const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"converttopsbt",
                "\nConverts a network serialized transaction to a PSBT. This should be used only with createrawtransaction and fundrawtransaction\n"
                "createpsbt and walletcreatefundedpsbt should be used for new applications.\n",
                {
                    {"hexstring", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The hex string of a raw transaction"},
                    {"permitsigdata", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "false", "If true, any signatures in the input will be discarded and conversion.\n"
                            "                              will continue. If false, RPC will fail if any signatures are present."},
                }}
                .ToString() +
                            "\nResult:\n"
                            "  \"psbt\"        (string)  The resulting raw transaction (base64-encoded string)\n"
                            "\nExamples:\n"
                            "\nCreate a transaction\n"
                            + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"[{\\\"data\\\":\\\"00010203\\\"}]\"") +
                            "\nConvert the transaction to a PSBT\n"
                            + HelpExampleCli("converttopsbt", "\"rawtransaction\"")
                            );
    }

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::MBOOL|UniValue::VNULL});

    // parse hex string from parameter
    CMutableTransaction tx;
    if (!DecodeHexTx(tx, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    // Remove all scriptSigs from inputs
    for (CTxIn &input : tx.vin) {
        if (!input.scriptSig.empty() &&
            (request.params[1].isNull() || request.params[1].get_bool())) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                               "Inputs must not have scriptSigs");
        }
        input.scriptSig.clear();
    }

    // Make a blank psbt
    PartiallySignedTransaction psbtx;
    psbtx.tx = tx;
    for (size_t i = 0; i < tx.vin.size(); ++i) {
        psbtx.inputs.push_back(PSBTInput());
    }
    for (size_t i = 0; i < tx.vout.size(); ++i) {
        psbtx.outputs.push_back(PSBTOutput());
    }

    // Serialize the PSBT
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << psbtx;

    return EncodeBase64(MakeUInt8Span(ssTx));
}

// clang-format off
static const ContextFreeRPCCommand commands[] = {
    //  category            name                         actor (function)           argNames
    //  ------------------- ------------------------     ----------------------     ----------
    { "rawtransactions",    "getrawtransaction",         getrawtransaction,         {"txid","verbose","blockhash"} },
    { "rawtransactions",    "createrawtransaction",      createrawtransaction,      {"inputs","outputs","locktime"} },
    { "rawtransactions",    "decoderawtransaction",      decoderawtransaction,      {"hexstring"} },
    { "rawtransactions",    "decodescript",              decodescript,              {"hexstring"} },
    { "rawtransactions",    "sendrawtransaction",        sendrawtransaction,        {"hexstring","allowhighfees"} },
    { "rawtransactions",    "combinerawtransaction",     combinerawtransaction,     {"txs"} },
    { "rawtransactions",    "signrawtransactionwithkey", signrawtransactionwithkey, {"hexstring","privkeys","prevtxs","sighashtype"} },
    { "rawtransactions",    "testmempoolaccept",         testmempoolaccept,         {"rawtxs","allowhighfees"} },
    { "rawtransactions",    "decodepsbt",                decodepsbt,                {"psbt"} },
    { "rawtransactions",    "combinepsbt",               combinepsbt,               {"txs"} },
    { "rawtransactions",    "finalizepsbt",              finalizepsbt,              {"psbt", "extract"} },
    { "rawtransactions",    "createpsbt",                createpsbt,                {"inputs","outputs","locktime"} },
    { "rawtransactions",    "converttopsbt",             converttopsbt,             {"hexstring","permitsigdata"} },

    { "blockchain",         "gettxoutproof",             gettxoutproof,             {"txids", "blockhash"} },
    { "blockchain",         "verifytxoutproof",          verifytxoutproof,          {"proof"} },
};
// clang-format on

void RegisterRawTransactionRPCCommands(CRPCTable &t) {
    for (unsigned int vcidx = 0; vcidx < std::size(commands); ++vcidx) {
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
    }
}
