// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <core_io.h>
#include <dsproof/dsproof.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <streams.h>
#include <txmempool.h>
#include <univalue.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <validation.h>

#include <algorithm>

namespace {

inline constexpr int verbosityMax = 3; ///< Max verbosity for ToObject() below

int ParseVerbosity(const UniValue &arg, const int def, const int trueval, const int falseval = 0) {
    int verbosity = def;

    if (arg.is(UniValue::VNUM)) {
        verbosity = arg.get_int();
    } else if (arg.is(UniValue::MBOOL)) {
        verbosity = arg.get_bool() ? trueval : falseval;
    }
    if (verbosity < 0 || verbosity > verbosityMax)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Bad verbosity");
    return verbosity;
}

UniValue::Object ToObject(const DoubleSpendProof::Spender &spender) {
    UniValue::Object ret;
    ret.reserve(7);
    ret.emplace_back("txversion", spender.txVersion);
    ret.emplace_back("sequence", spender.outSequence);
    ret.emplace_back("locktime", spender.lockTime);
    ret.emplace_back("hashprevoutputs", spender.hashPrevOutputs.ToString());
    ret.emplace_back("hashsequence", spender.hashSequence.ToString());
    ret.emplace_back("hashoutputs", spender.hashOutputs.ToString());
    UniValue::Object pushData;
    pushData.reserve(2);
    CScript script;
    for (const auto &data : spender.pushData)
        script << data;
    pushData.emplace_back("asm", ScriptToAsmStr(script, true));
    pushData.emplace_back("hex", HexStr(script));
    ret.emplace_back("pushdata", std::move(pushData));
    return ret;
}

UniValue::Object ToObject(int verbosity, const DoubleSpendProof &dsproof, const TxId &txId,
                          const std::optional<CTxMemPool::DspQueryPath> &path = std::nullopt,
                          const std::optional<CTxMemPool::DspDescendants> &descendants = std::nullopt) {
    UniValue::Object ret;

    if (verbosity <= 0 || verbosity > verbosityMax)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Bad verbosity");

    ret.reserve(1 + verbosity + bool(path) + bool(descendants));

    // verbosity = 1, dump an object with keys: "hex", "txid" (optionally "path", "descendants")
    if (verbosity == 1) {
        // add "hex" data blob
        std::vector<uint8_t> blob;
        CVectorWriter(SER_NETWORK, PROTOCOL_VERSION, blob, 0) << dsproof;
        ret.emplace_back("hex", HexStr(blob));
        // push txid=null for orphans
        ret.emplace_back("txid", !txId.IsNull() ? txId.ToString() : UniValue{});
    }
    // verbosity = 2 or above, dump an object with keys: "dspid", "txid", "outpoint" (optionally "path", "descendants")
    if (verbosity >= 2) {
        ret.emplace_back("dspid", dsproof.GetId().ToString());
        // push txid=null for orphans
        ret.emplace_back("txid", !txId.IsNull() ? txId.ToString() : UniValue{});
        UniValue::Object outpoint;
        outpoint.reserve(2);
        outpoint.emplace_back("txid", dsproof.prevTxId().ToString());
        outpoint.emplace_back("vout", dsproof.prevOutIndex());
        ret.emplace_back("outpoint", std::move(outpoint));
    }
    // add "path" key if the caller provided a valid optional
    if (path) {
        UniValue::Array arr;
        arr.reserve(path->size());
        for (const auto &ancestorTxId : *path)
            arr.emplace_back(ancestorTxId.ToString());
        ret.emplace_back("path", std::move(arr));
    }
    // add "descendants" key if the caller provided a valid optional
    if (descendants) {
        UniValue::Array arr;
        arr.reserve(descendants->size());
        for (const auto &descendantTxId : *descendants)
            arr.emplace_back(descendantTxId.ToString());
        ret.emplace_back("descendants", std::move(arr));
    }
    // verbosity = 3 or above, add the "spenders" array
    if (verbosity >= 3) {
        UniValue::Array spenders;
        spenders.reserve(2);
        for (const auto &spender : {dsproof.spender1(), dsproof.spender2()})
            spenders.emplace_back(ToObject(spender));
        ret.emplace_back("spenders", std::move(spenders));
    }
    return ret;
}

void ThrowIfDisabled() {
    if (!DoubleSpendProof::IsEnabled())
        throw JSONRPCError(RPC_METHOD_DISABLED,
                           "Double-spend proofs subsystem is disabled. Restart with -doublespendproof=1 to enable.");
}

std::string ResultsPartForVerbosity(int verbosity, std::string::size_type indent = 0) {
    std::string ret;
    switch (verbosity) {
    case 1:
        ret =
            "{                                (json object)\n"
            "  \"hex\" : \"xx\",                  (string) The raw serialized double-spend proof data.\n"
            "  \"txid\" : \"xx\"                  (string) The txid of the transaction associated with this "
            "double-spend. May be null for \"orphan\" double-spend proofs.\n"
            "}";
        break;
    case 2:
        ret =
            "{                                (json object)\n"
            "  \"dspid\" : \"xx\",                (string) Double-spend proof ID as a hex string.\n"
            "  \"txid\" : \"xx\",                 (string) The txid of the transaction associated with this "
            "double-spend. May be null for \"orphan\" double-spend proofs.\n"
            "  \"outpoint\" :                   (json object) The previous output (coin) that is being double-spent.\n"
            "  {\n"
            "    \"txid\" : \"xx\",               (string) The previous output txid.\n"
            "    \"vout\" : n ,                 (numeric) The previous output index number.\n"
            "  }\n"
            "}";
        break;
    case 3:
        ret =
            "  \"spenders\" :                   (json array of object) The conflicting spends.\n"
            "  [\n"
            "    {                            (json object)\n"
            "      \"txversion\" : n,           (numeric) Transaction version number.\n"
            "      \"sequence\" : n,            (numeric) Script sequence number.\n"
            "      \"locktime\" : n,            (numeric) Spending tx locktime.\n"
            "      \"hashprevoutputs\" : \"xx\",  (string) Hash of the previous outputs.\n"
            "      \"hashsequence\" : \"xx\",     (string) Hash of the sequence.\n"
            "      \"hashoutputs\" : \"xx\",      (string) Hash of the outputs.\n"
            "      \"pushdata\" :               (json object) Script signature push data.\n"
            "      {\n"
            "        \"asm\" : \"xx\",            (string) Script assembly representation.\n"
            "        \"hex\" : \"xx\"             (string) Script hex.\n"
            "      }\n"
            "    }, ...\n"
            "  ]";
        break;
    default:
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Invalid verbosity specified to internal method");
    }
    if (indent) {
        std::vector<std::string> lines;
        Split(lines, ret, "\n");
        const std::string indentStr(indent, ' ');
        const std::string lineSep = "\n" + indentStr;
        ret = indentStr + Join(lines, lineSep);
    }
    return ret;
}

} // namespace

static UniValue getdsproof(const Config &,
                           const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3) {
        throw std::runtime_error(
            RPCHelpMan{
                "getdsproof",
                "\nGet information for a double-spend proof.\n",
                {{"dspid_or_txid_or_outpoint", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "",
                  "The dspid, txid, or output point associated with the double-spend proof you wish to retrieve."
                  " Outpoints should be specified as a json object containing keys \"txid\" (string) and \"vout\""
                  " (numeric)."},
                 {"verbosity", RPCArg::Type::NUM, /* opt */ true, /* default_val */ "2",
                   "Values 0-3 return progressively more information for each increase in verbosity."
                   " This option may also be specified as a boolean where false is the same as verbosity=0 and true"
                   " is verbosity=2."},
                 {"recursive", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "true",
                   "If doing a lookup by txid, then search for a double-spend proof for all in-mempool ancestors of"
                   " txid as well. This option is ignored if not searching by txid."}},
                RPCResults{
                    RPCResult{"for verbosity = 0, 1, false",
                              ResultsPartForVerbosity(1) + "\n"},

                    RPCResult{"for verbosity = 2, true",
                              ResultsPartForVerbosity(2) + "\n"},

                    RPCResult{"additional keys if verbosity >= 1 and there is a non-orphan result",
                              "  ...\n"
                              "  \"descendants\" :                (json array of string) Set of all descendants of the"
                              " double-spend tx, including the double-spend tx.\n"
                              "  [\n"
                              "    \"txid\" :                     (string) Txid hex.\n"
                              "    , ...\n"
                              "  ]\n"
                              },

                    RPCResult{"additional keys if searching by txid and recursive = true",
                              "  ...\n"
                              "  \"path\" :                       (json array of string) Path from the query tx leading"
                              " up to and including the double-spend tx.\n"
                              "  [\n"
                              "    \"txid\" :                     (string) Txid hex, ordered by by child->parent.\n"
                              "    , ...\n"
                              "  ]\n"
                              },

                    RPCResult{"additional keys for verbosity = 3",
                              "  ...\n"
                              + ResultsPartForVerbosity(3) + "\n"},
                },
                RPCExamples{HelpExampleCli("getdsproof", "d3aac244e46f4bc5e2140a07496a179624b42d12600bfeafc358154ec89a720c false") +
                            HelpExampleCli("getdsproof", "fb5ae5344cb6995e529201fe24247ac38452f4e5ab5669b649e935853a7a180a") +
                            HelpExampleCli("getdsproof", "fb5ae5344cb6995e529201fe24247ac38452f4e5ab5669b649e935853a7a180a true true") +
                            HelpExampleCli("getdsproof", "fb5ae5344cb6995e529201fe24247ac38452f4e5ab5669b649e935853a7a180a 1 false") +
                            HelpExampleCli("getdsproof", "'{\"txid\": \"e66c1848fd3268a7d1cfac833f9164057805cc9b22ea5521d36dc4cf63f5fe83\", "
                                                         "\"vout\": 0}' true") +
                            HelpExampleRpc("getdsproof", "\"fb5ae5344cb6995e529201fe24247ac38452f4e5ab5669b649e935853a7a180a\", true, false") +
                            HelpExampleRpc("getdsproof", "{\"txid\": \"e66c1848fd3268a7d1cfac833f9164057805cc9b22ea5521d36dc4cf63f5fe83\", "
                                                         "\"vout\": 0}, true")},
            }.ToStringWithResultsAndExamples());
    }

    ThrowIfDisabled(); // don't proceed if the subsystem was disabled with -doublespendproof=0

    RPCTypeCheck(request.params, {UniValue::VSTR|UniValue::VOBJ,
                                  UniValue::VNUM|UniValue::MBOOL|UniValue::VNULL,
                                  UniValue::MBOOL|UniValue::VNULL});


    std::optional<CTxMemPool::DspTxIdPair> optPair;
    std::optional<CTxMemPool::DspQueryPath> optPath;
    std::optional<CTxMemPool::DspDescendants> optDescendants;
    CTxMemPool::DspDescendants *descPtr = nullptr;

    // parse second arg, either a bool or a number
    const int verbosity = ParseVerbosity(request.params[1], 2, 2);

    // parse third arg if present (default true)
    const bool recursive = request.params.size() > 2 ? request.params[2].get_bool() : true;

    // show descendants set for any result with verbosity >= 1
    if (verbosity >= 1) {
        optDescendants.emplace();
        descPtr = &*optDescendants;
    }

    // parse first arg -- either a hash string or a json object -- and do lookup based on it
    if (request.params[0].is(UniValue::VSTR)) {
        // lookup by dspid or txid
        const uint256 hash = ParseHashV(request.params[0], "dspid|txid");
        // first, try lookup by dspid
        optPair = g_mempool.getDoubleSpendProof(DspId(hash), descPtr);
        if (!optPair) {
            // not found by dspid, assume arg was a txid; try by txid
            TxId txId{hash};
            if (recursive) {
                // recursive search (the default)
                if (auto optResult = g_mempool.recursiveDSProofSearch(txId, descPtr)) {
                    // The double-spent txid is in path.back(), and `txId`
                    // is in path.front().
                    auto & [proof, path] = *optResult;
                    assert(!path.empty());
                    assert(path.front() == txId);
                    assert(!descPtr || !descPtr->empty());
                    assert(!descPtr || (descPtr->count(txId) && descPtr->count(path.back())));
                    optPair.emplace(std::move(proof), path.back()); // the proof goes with the double-spent txId
                    optPath.emplace(std::move(path));
                }
            } else {
                if (auto optProof = g_mempool.getDoubleSpendProof(txId, descPtr))
                    optPair.emplace(std::move(*optProof), std::move(txId));
            }
        }
    } else {
        // UniValue::VOBJ -- lookup by outpoint UniValue object "txid" and "vout" keys
        const UniValue::Object &obj = request.params[0].get_obj();
        RPCTypeCheckObj(obj, {{"txid", UniValue::VSTR},
                              {"vout", UniValue::VNUM}});
        const TxId txId{ParseHashO(obj, "txid")};
        const int vout = obj["vout"].get_int();
        if (vout < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "vout must be positive");
        const COutPoint outpoint(txId, static_cast<uint32_t>(vout));

        // next, look up the outpoint
        optPair = g_mempool.getDoubleSpendProof(outpoint, descPtr);
    }

    if (!optPair)
        return UniValue{}; // noting found, return null

    if (optPair->second.IsNull()) {
        // orphan result, ensure optPath and optDescendants empty to omit "descendants" and "path" keys
        optPath.reset();
        optDescendants.reset();
    }

    return ToObject(std::max(verbosity, 1), optPair->first, optPair->second, optPath, optDescendants);
}

static UniValue getdsprooflist(const Config &,
                               const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{
                "getdsprooflist",
                "\nList double-spend proofs for transactions in the mempool.\n",
                {{"verbosity", RPCArg::Type::NUM, /* opt */ true, /* default_val */ "0",
                  "Values 0-3 return progressively more information for each increase in verbosity."
                  " This option may also be specified as a boolean where false is the same as verbosity=0 and true"
                  " is verbosity=2."},
                 {"include_orphans", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "false",
                  "If true, then also include double-spend proofs that we know about but which are for transactions"
                  " that we don't yet have."}},
                RPCResults{
                    RPCResult{"for verbosity = 0 or false",
                              "[                                  (json array of string)\n"
                              "  \"dspid\"                          (string) Double-spend proof ID as a hex string.\n"
                              "  , ...\n"
                              "]\n"},

                    RPCResult{"for verbosity = 1",
                              "[                                  (json array of object)\n"
                              + ResultsPartForVerbosity(1, 2)
                              + ", ...\n"
                              "]\n"},

                    RPCResult{"for verbosity = 2 or true",
                              "[                                  (json array of object)\n"
                              + ResultsPartForVerbosity(2, 2)
                              + ", ...\n"
                              "]\n"},

                    RPCResult{"additional keys for verbosity = 3",
                              "    ...\n"
                              + ResultsPartForVerbosity(3, 2) + "\n"},
                },
                RPCExamples{HelpExampleCli("getdsprooflist", "2 false") +
                            HelpExampleCli("getdsprooflist", "false false") +
                            HelpExampleRpc("getdsprooflist", "1, true")}
            }.ToStringWithResultsAndExamples());
    }

    ThrowIfDisabled(); // don't proceed if the subsystem was disabled with -doublespendproof=0

    RPCTypeCheck(request.params, {UniValue::VNUM|UniValue::MBOOL|UniValue::VNULL,
                                  UniValue::MBOOL|UniValue::VNULL});

    const int verbosity = ParseVerbosity(request.params[0], 0, 2); // defaults to 0 (false) if missing, true=2
    bool includeOrphans = false;

    if (request.params.size() >= 2)
        includeOrphans = request.params[1].get_bool();

    const auto proofs = g_mempool.listDoubleSpendProofs(includeOrphans);
    UniValue::Array ret;
    ret.reserve(proofs.size());

    if (verbosity <= 0) {
        // verbosity = 0, just dump the dsproof id's
        for (const auto & [dsproof, txId] : proofs)
            ret.emplace_back(dsproof.GetId().ToString());
    } else if (verbosity >= 1) {
        // verbosity = 1, dump an object with keys: "txid", "hex"
        // verbosity = 2, dump an object with keys: "dspid", "txid", "outpoint",
        // verbosity = 3, dump an object with keys: "dspid", "txid", "outpoint", "spenders"
        for (const auto & [dsproof, txId] : proofs)
            ret.emplace_back(ToObject(verbosity, dsproof, txId));
    }
    return ret;
}

static UniValue getdsproofscore(const Config &,
                                const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{
                "getdsproofscore",
                "\nReturn a double-spend confidence score for a mempool transaction.\n",
                {{"txid", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "",
                  "The in-memory txid to query."},
                },
                RPCResults{
                    RPCResult{"n          (numeric) A value from 0.0 to 1.0, with 1.0 indicating that the\n"
                              "           transaction in question has no current dsproofs for it or any of its\n"
                              "           mempool ancestors, but that a future dsproof is possible. Confidence\n"
                              "           that this transaction has no known double-spends is relatively high.\n"
                              "\n"
                              "           A value of 0.0 indicates that the tx in question or one of its\n"
                              "           mempool ancestors has a dsproof, or that it or one of its mempool\n"
                              "           ancestors does not support dsproofs (not P2PKH), so confidence in\n"
                              "           this tx should be low.\n"
                              "\n"
                              "           A value of 0.25 indicates that up to the first 20,000 ancestors were\n"
                              "           checked and all have no proofs but *can* have proofs. Since the tx\n"
                              "           in question has a very large mempool ancestor set, double-spend\n"
                              "           confidence should be considered medium-to-low. (This value may also\n"
                              "           be returned for transactions which exceed depth 1,000 in an\n"
                              "           unconfirmed ancestor chain).\n"},
                },
                RPCExamples{HelpExampleCli("getdsproofscore", "d3aac244e46f4bc5e2140a07496a179624b42d12600bfeafc358154ec89a720c") +
                            HelpExampleRpc("getdsproofscore", "d3aac244e46f4bc5e2140a07496a179624b42d12600bfeafc358154ec89a720c")}
            }.ToStringWithResultsAndExamples());
    }

    ThrowIfDisabled(); // don't proceed if the subsystem was disabled with -doublespendproof=0

    RPCTypeCheck(request.params, {UniValue::VSTR});

    // lookup by txid only
    const TxId txId{ParseHashV(request.params[0], "txid")};

    double score{};

    try {
        g_mempool.recursiveDSProofSearch(txId, nullptr, &score);
    } catch (const CTxMemPool::RecursionLimitReached &e) {
        // ok, score will be set for us correctly anyway in this case
        LogPrint(BCLog::DSPROOF, "getdsproofscore (txid: %s) caught exception: %s\n", txId.ToString(), e.what());
    }

    if (score < 0.0) {
        /* A score of <0.0 means txid not found */
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not in mempool");
    }

    return score;
}

// clang-format off
static const ContextFreeRPCCommand commands[] = {
    //  category            name                      actor (function)        argNames
    //  ------------------- ------------------------  ----------------------  ----------
    { "blockchain",         "getdsproof",             getdsproof,             {"dspid|txid|outpoint", "verbosity|verbose", "recursive"} },
    { "blockchain",         "getdsprooflist",         getdsprooflist,         {"verbosity|verbose", "include_orphans"} },
    { "blockchain",         "getdsproofscore",        getdsproofscore,        {"txid"} },
};
// clang-format on

void RegisterDSProofRPCCommands(CRPCTable &t) {
    for (unsigned int vcidx = 0; vcidx < std::size(commands); ++vcidx) {
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
    }
}

