// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <config.h>
#include <core_io.h>
#include <interfaces/chain.h>
#include <key_io.h>
#include <merkleblock.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <script/script.h>
#include <script/standard.h>
#include <sync.h>
#include <util/string.h>
#include <util/system.h>
#include <util/time.h>
#include <validation.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>

#include <univalue.h>

#include <cstdint>
#include <fstream>
#include <optional>

static std::string EncodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (const uint8_t c : str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << HexStr(Span<const uint8_t>(&c, 1));
        } else {
            ret << c;
        }
    }
    return ret.str();
}

static std::string DecodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned int pos = 0; pos < str.length(); pos++) {
        uint8_t c = str[pos];
        if (c == '%' && pos + 2 < str.length()) {
            c = (((str[pos + 1] >> 6) * 9 + ((str[pos + 1] - '0') & 15)) << 4) |
                ((str[pos + 2] >> 6) * 9 + ((str[pos + 2] - '0') & 15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

bool GetWalletAddressesForKey(const Config &config, CWallet *const pwallet,
                              const CKeyID &keyid, std::string &strAddr,
                              std::string &strLabel) {
    bool fLabelFound = false;
    CKey key;
    pwallet->GetKey(keyid, key);
    for (const auto &dest : GetAllDestinationsForKey(key.GetPubKey())) {
        if (pwallet->mapAddressBook.count(dest)) {
            if (!strAddr.empty()) {
                strAddr += ",";
            }
            strAddr += EncodeDestination(dest, config);
            strLabel = EncodeDumpString(pwallet->mapAddressBook[dest].name);
            fLabelFound = true;
        }
    }
    if (!fLabelFound) {
        strAddr = EncodeDestination(
            GetDestinationForKey(key.GetPubKey(),
                                 pwallet->m_default_address_type),
            config);
    }
    return fLabelFound;
}

static const int64_t TIMESTAMP_MIN = 0;

static void RescanWallet(CWallet &wallet, const WalletRescanReserver &reserver,
                         int64_t time_begin = TIMESTAMP_MIN,
                         bool update = true) {
    int64_t scanned_time = wallet.RescanFromTime(time_begin, reserver, update);
    if (wallet.IsAbortingRescan()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Rescan aborted by user.");
    } else if (scanned_time > time_begin) {
        throw JSONRPCError(RPC_WALLET_ERROR,
                           "Rescan was unable to fully rescan the blockchain. "
                           "Some transactions may be missing.");
    }
}

UniValue importprivkey(const Config &, const JSONRPCRequest &request) {
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet *const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return UniValue();
    }

    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 3) {
        throw std::runtime_error(
            RPCHelpMan{"importprivkey",
                "\nAdds a private key (as returned by dumpprivkey) to your wallet. Requires a new wallet backup.\n"
                "Hint: use importmulti to import more than one private key.\n",
                {
                    {"privkey", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "The private key (see dumpprivkey)"},
                    {"label", RPCArg::Type::STR, /* opt */ true, /* default_val */ "current label if address exists, otherwise \"\"", "An optional label"},
                    {"rescan", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "true", "Rescan the wallet for transactions"},
                }}
                .ToString() +
            "\nNote: This call can take minutes to complete if rescan is true, "
            "during that time, other rpc calls\n"
            "may report that the imported key exists but related transactions "
            "are still missing, leading to temporarily incorrect/bogus "
            "balances and unspent outputs until rescan completes.\n"
            "\nExamples:\n"
            "\nDump a private key\n" +
            HelpExampleCli("dumpprivkey", "\"myaddress\"") +
            "\nImport the private key with rescan\n" +
            HelpExampleCli("importprivkey", "\"mykey\"") +
            "\nImport using a label and without rescan\n" +
            HelpExampleCli("importprivkey", "\"mykey\" \"testing\" false") +
            "\nImport using default blank label and without rescan\n" +
            HelpExampleCli("importprivkey", "\"mykey\" \"\" false") +
            "\nAs a JSON-RPC call\n" +
            HelpExampleRpc("importprivkey", "\"mykey\", \"testing\", false"));
    }

    if (pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        throw JSONRPCError(RPC_WALLET_ERROR,
                           "Cannot import private keys to a wallet with "
                           "private keys disabled");
    }

    WalletRescanReserver reserver(pwallet);
    bool fRescan = true;
    {
        auto locked_chain = pwallet->chain().lock();
        LOCK(pwallet->cs_wallet);

        EnsureWalletIsUnlocked(pwallet);

        std::string strSecret = request.params[0].get_str();
        std::string strLabel = "";
        if (!request.params[1].isNull()) {
            strLabel = request.params[1].get_str();
        }

        // Whether to perform rescan after import
        if (!request.params[2].isNull()) {
            fRescan = request.params[2].get_bool();
        }

        if (fRescan && fPruneMode) {
            throw JSONRPCError(RPC_WALLET_ERROR,
                               "Rescan is disabled in pruned mode");
        }

        if (fRescan && !reserver.reserve()) {
            throw JSONRPCError(RPC_WALLET_ERROR,
                               "Wallet is currently rescanning. Abort existing "
                               "rescan or wait.");
        }

        CKey key = DecodeSecret(strSecret);
        if (!key.IsValid()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                               "Invalid private key encoding");
        }

        CPubKey pubkey = key.GetPubKey();
        assert(key.VerifyPubKey(pubkey));
        CKeyID vchAddress = pubkey.GetID();
        {
            pwallet->MarkDirty();
            // We don't know which corresponding address will be used; label
            // them all
            for (const auto &dest : GetAllDestinationsForKey(pubkey)) {
                pwallet->SetAddressBook(dest, strLabel, "receive");
            }

            // Don't throw error in case a key is already there
            if (pwallet->HaveKey(vchAddress)) {
                return UniValue();
            }

            pwallet->LearnAllRelatedScripts(pubkey);

            // whenever a key is imported, we need to scan the whole chain
            pwallet->UpdateTimeFirstKey(1);
            pwallet->mapKeyMetadata[vchAddress].nCreateTime = 1;

            if (!pwallet->AddKeyPubKey(key, pubkey)) {
                throw JSONRPCError(RPC_WALLET_ERROR,
                                   "Error adding key to wallet");
            }
        }
    }
    if (fRescan) {
        RescanWallet(*pwallet, reserver);
    }

    return UniValue();
}

UniValue abortrescan(const Config &, const JSONRPCRequest &request) {
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet *const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return UniValue();
    }

    if (request.fHelp || request.params.size() > 0) {
        throw std::runtime_error(
            RPCHelpMan{"abortrescan",
                "\nStops current wallet rescan triggered by an RPC call, e.g. by an importprivkey call.\n", {}}
                .ToString() +
            "\nExamples:\n"
            "\nImport a private key\n"
            + HelpExampleCli("importprivkey", "\"mykey\"") +
            "\nAbort the running wallet rescan\n"
            + HelpExampleCli("abortrescan", "") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("abortrescan", "")
        );
    }

    if (!pwallet->IsScanning() || pwallet->IsAbortingRescan()) {
        return false;
    }
    pwallet->AbortRescan();
    return true;
}

static void ImportAddress(CWallet *, const CTxDestination &dest,
                          const std::string &strLabel);
static void ImportScript(CWallet *const pwallet, const CScript &script,
                         const std::string &strLabel, bool isRedeemScript)
    EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet) {
    if (!isRedeemScript && ::IsMine(*pwallet, script) == ISMINE_SPENDABLE) {
        throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already contains the "
                                             "private key for this address or "
                                             "script");
    }

    pwallet->MarkDirty();

    if (!pwallet->HaveWatchOnly(script) &&
        !pwallet->AddWatchOnly(script, 0 /* nCreateTime */)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");
    }

 
    CTxDestination destination;
    if (ExtractDestination(script, destination)) {
        pwallet->SetAddressBook(destination, strLabel, "receive");
    }
 
}

static void ImportAddress(CWallet *const pwallet, const CTxDestination &dest,
                          const std::string &strLabel)
    EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet) {
    CScript script = GetScriptForDestination(dest);
    ImportScript(pwallet, script, strLabel, false);
    // add to address book or update label
    if (IsValidDestination(dest)) {
        pwallet->SetAddressBook(dest, strLabel, "receive");
    }
}

UniValue importaddress(const Config &config, const JSONRPCRequest &request) {
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet *const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return UniValue();
    }

    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 4) {
        throw std::runtime_error(
            RPCHelpMan{"importaddress",
                "\nAdds an address or script (in hex) that can be watched as if it were in your wallet but cannot be used to spend. Requires a new wallet backup.\n",
                {
                    {"address", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "The Radiant address (or hex-encoded script)"},
                    {"label", RPCArg::Type::STR, /* opt */ true, /* default_val */ "\"\"", "An optional label"},
                    {"rescan", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "true", "Rescan the wallet for transactions"},
                }}
                .ToString() +
            "\nNote: This call can take minutes to complete if rescan is true, "
            "during that time, other rpc calls\n"
            "may report that the imported address exists but related "
            "transactions are still missing, leading to temporarily "
            "incorrect/bogus balances and unspent outputs until rescan "
            "completes.\n"
            "If you have the full public key, you should call importpubkey "
            "instead of this.\n"
            "\nNote: If you import a non-standard raw script in hex form, "
            "outputs sending to it will be treated\n"
            "as change, and not show up in many RPCs.\n"
            "\nExamples:\n"
            "\nImport an address with rescan\n" +
            HelpExampleCli("importaddress", "\"myaddress\"") +
            "\nImport using a label without rescan\n" +
            HelpExampleCli("importaddress", "\"myaddress\" \"testing\" false") +
            "\nAs a JSON-RPC call\n" +
            HelpExampleRpc("importaddress",
                           "\"myaddress\", \"testing\", false"));
    }

    std::string strLabel;
    if (!request.params[1].isNull()) {
        strLabel = request.params[1].get_str();
    }

    // Whether to perform rescan after import
    bool fRescan = true;
    if (!request.params[2].isNull()) {
        fRescan = request.params[2].get_bool();
    }

    if (fRescan && fPruneMode) {
        throw JSONRPCError(RPC_WALLET_ERROR,
                           "Rescan is disabled in pruned mode");
    }

    WalletRescanReserver reserver(pwallet);
    if (fRescan && !reserver.reserve()) {
        throw JSONRPCError(
            RPC_WALLET_ERROR,
            "Wallet is currently rescanning. Abort existing rescan or wait.");
    }
    {
        auto locked_chain = pwallet->chain().lock();
        LOCK(pwallet->cs_wallet);

        CTxDestination dest = DecodeDestination(request.params[0].get_str(),
                                                config.GetChainParams());
        if (IsValidDestination(dest)) {
            ImportAddress(pwallet, dest, strLabel);
        } else if (IsHex(request.params[0].get_str())) {
            std::vector<uint8_t> data(ParseHex(request.params[0].get_str()));
            ImportScript(pwallet, CScript(data.begin(), data.end()), strLabel,
                         false);
        } else {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                               "Invalid Radiant address or script");
        }
    }
    if (fRescan) {
        RescanWallet(*pwallet, reserver);
        pwallet->ReacceptWalletTransactions();
    }

    return UniValue();
}

UniValue importprunedfunds(const Config &,
                           const JSONRPCRequest &request) {
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet *const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return UniValue();
    }

    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            RPCHelpMan{"importprunedfunds",
                "\nImports funds without rescan. Corresponding address or script must previously be included in wallet. Aimed towards pruned wallets. The end-user is responsible to import additional transactions that subsequently spend the imported outputs or rescan after the point in the blockchain the transaction is included.\n",
                {
                    {"rawtransaction", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "A raw transaction in hex funding an already-existing address in wallet"},
                    {"txoutproof", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The hex output from gettxoutproof that contains the transaction"},
                }}
                .ToString()
        );
    }

    CMutableTransaction tx;
    if (!DecodeHexTx(tx, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }
    uint256 txid = tx.GetId();
    CWalletTx wtx(pwallet, MakeTransactionRef(std::move(tx)));

    CDataStream ssMB(ParseHexV(request.params[1], "proof"), SER_NETWORK,
                     PROTOCOL_VERSION);
    CMerkleBlock merkleBlock;
    ssMB >> merkleBlock;

    // Search partial merkle tree in proof for our transaction and index in
    // valid block
    std::vector<uint256> vMatch;
    std::vector<size_t> vIndex;
    size_t txnIndex = 0;
    if (merkleBlock.txn.ExtractMatches(vMatch, vIndex) ==
        merkleBlock.header.hashMerkleRoot) {
        auto locked_chain = pwallet->chain().lock();
        if (locked_chain->getBlockHeight(merkleBlock.header.GetHash()) == std::nullopt) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                               "Block not found in chain");
        }

        std::vector<uint256>::const_iterator it;
        if ((it = std::find(vMatch.begin(), vMatch.end(), txid)) ==
            vMatch.end()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                               "Transaction given doesn't exist in proof");
        }

        txnIndex = vIndex[it - vMatch.begin()];
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           "Something wrong with merkleblock");
    }

    wtx.nIndex = txnIndex;
    wtx.hashBlock = merkleBlock.header.GetHash();

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    if (pwallet->IsMine(*wtx.tx)) {
        pwallet->AddToWallet(wtx, false);
        return UniValue();
    }

    throw JSONRPCError(
        RPC_INVALID_ADDRESS_OR_KEY,
        "No addresses in wallet correspond to included transaction");
}

UniValue removeprunedfunds(const Config &,
                           const JSONRPCRequest &request) {
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet *const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return UniValue();
    }

    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"removeprunedfunds",
                "\nDeletes the specified transaction from the wallet. Meant for use with pruned wallets and as a companion to importprunedfunds. This will affect wallet balances.\n",
                {
                    {"txid", RPCArg::Type::STR_HEX, /* opt */ false, /* default_val */ "", "The hex-encoded id of the transaction you are deleting"},
                }}
                .ToString() +
            "\nExamples:\n" +
            HelpExampleCli("removeprunedfunds", "\"a8d0c0184dde994a09ec054286f1"
                                                "ce581bebf46446a512166eae762873"
                                                "4ea0a5\"") +
            "\nAs a JSON-RPC call\n" +
            HelpExampleRpc("removeprunedfunds",
                           "\"a8d0c0184dde994a09ec054286f1ce581bebf46446a512166"
                           "eae7628734ea0a5\""));
    }

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    TxId txid(ParseHashV(request.params[0], "txid"));
    std::vector<TxId> txIds;
    txIds.push_back(txid);
    std::vector<TxId> txIdsOut;

    if (pwallet->ZapSelectTx(txIds, txIdsOut) != DBErrors::LOAD_OK) {
        throw JSONRPCError(RPC_WALLET_ERROR,
                           "Could not properly delete the transaction.");
    }

    if (txIdsOut.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Transaction does not exist in wallet.");
    }

    return UniValue();
}

UniValue importpubkey(const Config &, const JSONRPCRequest &request) {
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet *const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return UniValue();
    }

    if (request.fHelp || request.params.size() < 1 ||
        request.params.size() > 4) {
        throw std::runtime_error(
            RPCHelpMan{"importpubkey",
                "\nAdds a public key (in hex) that can be watched as if it were in your wallet but cannot be used to spend. Requires a new wallet backup.\n",
                {
                    {"pubkey", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "The hex-encoded public key"},
                    {"label", RPCArg::Type::STR, /* opt */ true, /* default_val */ "\"\"", "An optional label"},
                    {"rescan", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "true", "Rescan the wallet for transactions"},
                }}
                .ToString() +
            "\nNote: This call can take minutes to complete if rescan is true, "
            "during that time, other rpc calls\n"
            "may report that the imported pubkey exists but related "
            "transactions are still missing, leading to temporarily "
            "incorrect/bogus balances and unspent outputs until rescan "
            "completes.\n"
            "\nExamples:\n"
            "\nImport a public key with rescan\n" +
            HelpExampleCli("importpubkey", "\"mypubkey\"") +
            "\nImport using a label without rescan\n" +
            HelpExampleCli("importpubkey", "\"mypubkey\" \"testing\" false") +
            "\nAs a JSON-RPC call\n" +
            HelpExampleRpc("importpubkey", "\"mypubkey\", \"testing\", false"));
    }

    std::string strLabel;
    if (!request.params[1].isNull()) {
        strLabel = request.params[1].get_str();
    }

    // Whether to perform rescan after import
    bool fRescan = true;
    if (!request.params[2].isNull()) {
        fRescan = request.params[2].get_bool();
    }

    if (fRescan && fPruneMode) {
        throw JSONRPCError(RPC_WALLET_ERROR,
                           "Rescan is disabled in pruned mode");
    }

    WalletRescanReserver reserver(pwallet);
    if (fRescan && !reserver.reserve()) {
        throw JSONRPCError(
            RPC_WALLET_ERROR,
            "Wallet is currently rescanning. Abort existing rescan or wait.");
    }

    if (!IsHex(request.params[0].get_str())) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           "Pubkey must be a hex string");
    }
    std::vector<uint8_t> data(ParseHex(request.params[0].get_str()));
    CPubKey pubKey(data.begin(), data.end());
    if (!pubKey.IsFullyValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           "Pubkey is not a valid public key");
    }

    {
        auto locked_chain = pwallet->chain().lock();
        LOCK(pwallet->cs_wallet);

        for (const auto &dest : GetAllDestinationsForKey(pubKey)) {
            ImportAddress(pwallet, dest, strLabel);
        }
        ImportScript(pwallet, GetScriptForRawPubKey(pubKey), strLabel, false);
        pwallet->LearnAllRelatedScripts(pubKey);
    }
    if (fRescan) {
        RescanWallet(*pwallet, reserver);
        pwallet->ReacceptWalletTransactions();
    }

    return UniValue();
}

UniValue importwallet(const Config &config, const JSONRPCRequest &request) {
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet *const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return UniValue();
    }

    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"importwallet",
                "\nImports keys from a wallet dump file (see dumpwallet). Requires a new wallet backup to include imported keys.\n",
                {
                    {"filename", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "The wallet file"},
                }}
                .ToString() +
            "\nExamples:\n"
            "\nDump the wallet\n" +
            HelpExampleCli("dumpwallet", "\"test\"") + "\nImport the wallet\n" +
            HelpExampleCli("importwallet", "\"test\"") +
            "\nImport using the json rpc call\n" +
            HelpExampleRpc("importwallet", "\"test\""));
    }

    if (fPruneMode) {
        throw JSONRPCError(RPC_WALLET_ERROR,
                           "Importing wallets is disabled in pruned mode");
    }

    WalletRescanReserver reserver(pwallet);
    if (!reserver.reserve()) {
        throw JSONRPCError(
            RPC_WALLET_ERROR,
            "Wallet is currently rescanning. Abort existing rescan or wait.");
    }

    int64_t nTimeBegin = 0;
    bool fGood = true;
    {
        auto locked_chain = pwallet->chain().lock();
        LOCK(pwallet->cs_wallet);

        EnsureWalletIsUnlocked(pwallet);

        std::ifstream file;
        file.open(request.params[0].get_str().c_str(),
                  std::ios::in | std::ios::ate);
        if (!file.is_open()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                               "Cannot open wallet dump file");
        }
        std::optional<int> tip_height = locked_chain->getHeight();
        nTimeBegin = tip_height ? locked_chain->getBlockTime(*tip_height) : 0;

        int64_t nFilesize = std::max<int64_t>(1, file.tellg());
        file.seekg(0, file.beg);

        // Use uiInterface.ShowProgress instead of pwallet.ShowProgress because
        // pwallet.ShowProgress has a cancel button tied to AbortRescan which we
        // don't want for this progress bar showing the import progress.
        // uiInterface.ShowProgress does not have a cancel button.

        // show progress dialog in GUI
        uiInterface.ShowProgress(
            strprintf("%s " + _("Importing..."), pwallet->GetDisplayName()), 0,
            false);
        std::vector<std::tuple<CKey, int64_t, bool, std::string>> keys;
        std::vector<std::pair<CScript, int64_t>> scripts;
        while (file.good()) {
            uiInterface.ShowProgress(
                "",
                std::max(1, std::min<int>(50, 100 * double(file.tellg()) /
                                                  double(nFilesize))),
                false);
            std::string line;
            std::getline(file, line);
            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::vector<std::string> vstr;
            Split(vstr, line, " ");
            if (vstr.size() < 2) {
                continue;
            }
            CKey key = DecodeSecret(vstr[0]);
            if (key.IsValid()) {
                int64_t nTime = ParseISO8601DateTime(vstr[1]);
                std::string strLabel;
                bool fLabel = true;
                for (size_t nStr = 2; nStr < vstr.size(); nStr++) {
                    if (vstr[nStr].front() == '#') {
                        break;
                    }
                    if (vstr[nStr] == "change=1") {
                        fLabel = false;
                    }
                    if (vstr[nStr] == "reserve=1") {
                        fLabel = false;
                    }
                    if (vstr[nStr].substr(0, 6) == "label=") {
                        strLabel = DecodeDumpString(vstr[nStr].substr(6));
                        fLabel = true;
                    }
                }
                keys.push_back(std::make_tuple(key, nTime, fLabel, strLabel));
            } else if (IsHex(vstr[0])) {
                std::vector<uint8_t> vData(ParseHex(vstr[0]));
                CScript script = CScript(vData.begin(), vData.end());
                int64_t birth_time = ParseISO8601DateTime(vstr[1]);
                scripts.push_back(
                    std::pair<CScript, int64_t>(script, birth_time));
            }
        }
        file.close();
        // We now know whether we are importing private keys, so we can error if
        // private keys are disabled
        if (keys.size() > 0 &&
            pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
            // hide progress dialog in GUI
            uiInterface.ShowProgress("", 100, false);
            throw JSONRPCError(
                RPC_WALLET_ERROR,
                "Importing wallets is disabled when private keys are disabled");
        }
        double total = double(keys.size() + scripts.size());
        double progress = 0;
        for (const auto &key_tuple : keys) {
            uiInterface.ShowProgress(
                "",
                std::max(50, std::min<int>(75, 100 * progress / total) + 50),
                false);
            const CKey &key = std::get<0>(key_tuple);
            int64_t time = std::get<1>(key_tuple);
            bool has_label = std::get<2>(key_tuple);
            std::string label = std::get<3>(key_tuple);

            CPubKey pubkey = key.GetPubKey();
            assert(key.VerifyPubKey(pubkey));
            CKeyID keyid = pubkey.GetID();
            if (pwallet->HaveKey(keyid)) {
                pwallet->WalletLogPrintf(
                    "Skipping import of %s (key already present)\n",
                    EncodeDestination(keyid, config));
                continue;
            }
            pwallet->WalletLogPrintf("Importing %s...\n",
                                     EncodeDestination(keyid, config));
            if (!pwallet->AddKeyPubKey(key, pubkey)) {
                fGood = false;
                continue;
            }
            pwallet->mapKeyMetadata[keyid].nCreateTime = time;
            if (has_label) {
                pwallet->SetAddressBook(keyid, label, "receive");
            }
            nTimeBegin = std::min(nTimeBegin, time);
            progress++;
        }
        for (const auto &script_pair : scripts) {
            uiInterface.ShowProgress(
                "",
                std::max(50, std::min<int>(75, 100 * progress / total) + 50),
                false);
            const CScript &script = script_pair.first;
            int64_t time = script_pair.second;
            CScriptID id(script);
            if (pwallet->HaveCScript(id)) {
                pwallet->WalletLogPrintf(
                    "Skipping import of %s (script already present)\n",
                    HexStr(script));
                continue;
            }
            if (!pwallet->AddCScript(script)) {
                pwallet->WalletLogPrintf("Error importing script %s\n",
                                         HexStr(script));
                fGood = false;
                continue;
            }
            if (time > 0) {
                pwallet->m_script_metadata[id].nCreateTime = time;
                nTimeBegin = std::min(nTimeBegin, time);
            }
            progress++;
        }
        // hide progress dialog in GUI
        uiInterface.ShowProgress("", 100, false);
        pwallet->UpdateTimeFirstKey(nTimeBegin);
    }
    // hide progress dialog in GUI
    uiInterface.ShowProgress("", 100, false);
    RescanWallet(*pwallet, reserver, nTimeBegin, false /* update */);
    pwallet->MarkDirty();

    if (!fGood) {
        throw JSONRPCError(RPC_WALLET_ERROR,
                           "Error adding some keys/scripts to wallet");
    }

    return UniValue();
}

UniValue dumpprivkey(const Config &config, const JSONRPCRequest &request) {
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet *const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return UniValue();
    }

    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"dumpprivkey",
                "\nReveals the private key corresponding to 'address'.\n"
                "Then the importprivkey can be used with this output\n",
                {
                    {"address", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "The Radiant address for the private key"},
                }}
                .ToString() +
            "\nResult:\n"
            "\"key\"                (string) The private key\n"
            "\nExamples:\n" +
            HelpExampleCli("dumpprivkey", "\"myaddress\"") +
            HelpExampleCli("importprivkey", "\"mykey\"") +
            HelpExampleRpc("dumpprivkey", "\"myaddress\""));
    }

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    std::string strAddress = request.params[0].get_str();
    CTxDestination dest =
        DecodeDestination(strAddress, config.GetChainParams());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           "Invalid Radiant address");
    }
    auto keyid = GetKeyForDestination(*pwallet, dest);
    if (keyid.IsNull()) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    }
    CKey vchSecret;
    if (!pwallet->GetKey(keyid, vchSecret)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " +
                                                 strAddress + " is not known");
    }
    return EncodeSecret(vchSecret);
}

UniValue dumpwallet(const Config &config, const JSONRPCRequest &request) {
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet *const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return UniValue();
    }

    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            RPCHelpMan{"dumpwallet",
                "\nDumps all wallet keys in a human-readable format to a server-side file. This does not allow overwriting existing files.\n"
                "Imported scripts are included in the dumpfile, but corresponding addresses may not be added automatically by importwallet.\n"
                "Note that if your wallet contains keys which are not derived from your HD seed (e.g. imported keys), these are not covered by\n"
                "only backing up the seed itself, and must be backed up too (e.g. ensure you back up the whole dumpfile).\n",
                {
                    {"filename", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "The filename with path (either absolute or relative to bitcoind)"},
                }}
                .ToString() +
            "\nResult:\n"
            "{                           (json object)\n"
            "  \"filename\" : {        (string) The filename with full "
            "absolute path\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("dumpwallet", "\"test\"") +
            HelpExampleRpc("dumpwallet", "\"test\""));
    }

    auto locked_chain = pwallet->chain().lock();
    LOCK(pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    fs::path filepath = request.params[0].get_str();
    filepath = fs::absolute(filepath);

    /**
     * Prevent arbitrary files from being overwritten. There have been reports
     * that users have overwritten wallet files this way:
     * https://github.com/bitcoin/bitcoin/issues/9934
     * It may also avoid other security issues.
     */
    if (fs::exists(filepath)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           filepath.string() + " already exists. If you are "
                                               "sure this is what you want, "
                                               "move it out of the way first");
    }

    std::ofstream file;
    file.open(filepath.string().c_str());
    if (!file.is_open()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Cannot open wallet dump file");
    }

    std::map<CTxDestination, int64_t> mapKeyBirth;
    const std::map<CKeyID, int64_t> &mapKeyPool = pwallet->GetAllReserveKeys();
    pwallet->GetKeyBirthTimes(*locked_chain, mapKeyBirth);

    std::set<CScriptID> scripts = pwallet->GetCScripts();
    // TODO: include scripts in GetKeyBirthTimes() output instead of separate

    // sort time/key pairs
    std::vector<std::pair<int64_t, CKeyID>> vKeyBirth;
    for (const auto &entry : mapKeyBirth) {
        if (const CKeyID *keyID = boost::get<CKeyID>(&entry.first)) {
            // set and test
            vKeyBirth.push_back(std::make_pair(entry.second, *keyID));
        }
    }
    mapKeyBirth.clear();
    std::sort(vKeyBirth.begin(), vKeyBirth.end());

    // produce output
    file << strprintf("# Wallet dump created by Bitcoin %s\n", CLIENT_BUILD);
    file << strprintf("# * Created on %s\n", FormatISO8601DateTime(GetTime()));
    const std::optional<int> tip_height = locked_chain->getHeight();
    file << strprintf("# * Best block at time of backup was %i (%s),\n",
                      tip_height.value_or(-1),
                      tip_height
                          ? locked_chain->getBlockHash(*tip_height).ToString()
                          : "(missing block hash)");
    file << strprintf("#   mined on %s\n",
                      tip_height ? FormatISO8601DateTime(
                                       locked_chain->getBlockTime(*tip_height))
                                 : "(missing block time)");
    file << "\n";

    // add the base58check encoded extended master if the wallet uses HD
    CKeyID seed_id = pwallet->GetHDChain().seed_id;
    if (!seed_id.IsNull()) {
        CKey seed;
        if (pwallet->GetKey(seed_id, seed)) {
            CExtKey masterKey;
            masterKey.SetSeed(seed.begin(), seed.size());

            file << "# extended private masterkey: " << EncodeExtKey(masterKey)
                 << "\n\n";
        }
    }
    for (std::vector<std::pair<int64_t, CKeyID>>::const_iterator it =
             vKeyBirth.begin();
         it != vKeyBirth.end(); it++) {
        const CKeyID &keyid = it->second;
        std::string strTime = FormatISO8601DateTime(it->first);
        std::string strAddr;
        std::string strLabel;
        CKey key;
        if (pwallet->GetKey(keyid, key)) {
            file << strprintf("%s %s ", EncodeSecret(key), strTime);
            if (GetWalletAddressesForKey(config, pwallet, keyid, strAddr,
                                         strLabel)) {
                file << strprintf("label=%s", strLabel);
            } else if (keyid == seed_id) {
                file << "hdseed=1";
            } else if (mapKeyPool.count(keyid)) {
                file << "reserve=1";
            } else if (pwallet->mapKeyMetadata[keyid].hdKeypath == "s") {
                file << "inactivehdseed=1";
            } else {
                file << "change=1";
            }
            file << strprintf(
                " # addr=%s%s\n", strAddr,
                (pwallet->mapKeyMetadata[keyid].hdKeypath.size() > 0
                     ? " hdkeypath=" + pwallet->mapKeyMetadata[keyid].hdKeypath
                     : ""));
        }
    }
    file << "\n";
    for (const CScriptID &scriptid : scripts) {
        CScript script;
        std::string create_time = "0";
        std::string address = EncodeDestination(scriptid, config);
        // get birth times for scripts with metadata
        auto it = pwallet->m_script_metadata.find(scriptid);
        if (it != pwallet->m_script_metadata.end()) {
            create_time = FormatISO8601DateTime(it->second.nCreateTime);
        }
        if (pwallet->GetCScript(scriptid, script)) {
            file << strprintf("%s %s script=1", HexStr(script), create_time);
            file << strprintf(" # addr=%s\n", address);
        }
    }
    file << "\n";
    file << "# End of dump\n";
    file.close();

    UniValue::Object reply;
    reply.reserve(1);
    reply.emplace_back("filename", filepath.string());
    return reply;
}

static UniValue ProcessImport(CWallet *const pwallet, const UniValue &data,
                              const int64_t timestamp)
    EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet) {
    try {
        bool success = false;

        // Required fields.
        const UniValue &scriptPubKey = data["scriptPubKey"];

        // Should have script or JSON with "address".
        bool isScript = scriptPubKey.getType() == UniValue::VSTR;
        const UniValue* addressUV = scriptPubKey.getType() == UniValue::VOBJ ? scriptPubKey.locate("address") : nullptr;
        if (!addressUV && !isScript) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid scriptPubKey");
        }

        // Optional fields.
        const UniValue::Array emptyArray;
        const auto redeemscriptUV = data.locate("redeemscript");
        const std::string &strRedeemScript = redeemscriptUV ? redeemscriptUV->get_str() : "";
        const auto pubkeysUV = data.locate("pubkeys");
        const UniValue::Array &pubKeys = pubkeysUV ? pubkeysUV->get_array() : emptyArray;
        const auto keysUV = data.locate("keys");
        const UniValue::Array &keys = keysUV ? keysUV->get_array() : emptyArray;
        const auto internalUV = data.locate("internal");
        const bool internal = internalUV ? internalUV->get_bool() : false;
        const auto watchonlyUV = data.locate("watchonly");
        const bool watchOnly = watchonlyUV ? watchonlyUV->get_bool() : false;
        const auto labelUV = data.locate("label");
        const std::string &label = labelUV && !internal ? labelUV->get_str() : "";

        // If private keys are disabled, abort if private keys are being
        // imported
        if (pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS) &&
            keysUV) {
            throw JSONRPCError(RPC_WALLET_ERROR,
                               "Cannot import private keys to a wallet with "
                               "private keys disabled");
        }

        const std::string &output = isScript
                                        ? scriptPubKey.get_str()
                                        : addressUV->get_str();

        // Parse the output.
        CScript script;
        CTxDestination dest;

        if (!isScript) {
            dest = DecodeDestination(output, pwallet->chainParams);
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                                   "Invalid address");
            }
            script = GetScriptForDestination(dest);
        } else {
            if (!IsHex(output)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                                   "Invalid scriptPubKey");
            }

            std::vector<uint8_t> vData(ParseHex(output));
            script = CScript(vData.begin(), vData.end());
            if (!ExtractDestination(script, dest) && !internal) {
                throw JSONRPCError(RPC_INVALID_PARAMETER,
                                   "Internal must be set to true for "
                                   "nonstandard scriptPubKey imports.");
            }
        }

        // Watchonly and private keys
        if (watchOnly && keys.size()) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "Incompatibility found between watchonly and keys");
        }

        // Internal + Label
        if (internal && labelUV) {
            throw JSONRPCError(
                RPC_INVALID_PARAMETER,
                "Incompatibility found between internal and label");
        }

        // Keys / PubKeys size check.
        if ( (keys.size() > 1 || pubKeys.size() > 1)) { // Address / scriptPubKey
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                               "More than private key given for one address");
        }

        // Process. //
 
        // Import public keys.
        if (pubKeys.size() && keys.size() == 0) {
            const std::string &strPubKey = pubKeys[0].get_str();

            if (!IsHex(strPubKey)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                                    "Pubkey must be a hex string");
            }

            std::vector<uint8_t> vData(ParseHex(strPubKey));
            CPubKey pubKey(vData.begin(), vData.end());

            if (!pubKey.IsFullyValid()) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                                    "Pubkey is not a valid public key");
            }

            CTxDestination pubkey_dest = pubKey.GetID();

            // Consistency check.
            if (!(pubkey_dest == dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                                    "Consistency check failed");
            }

            CScript pubKeyScript = GetScriptForDestination(pubkey_dest);

            if (::IsMine(*pwallet, pubKeyScript) == ISMINE_SPENDABLE) {
                throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already "
                                                        "contains the private "
                                                        "key for this address "
                                                        "or script");
            }

            pwallet->MarkDirty();

            if (!pwallet->AddWatchOnly(pubKeyScript, timestamp)) {
                throw JSONRPCError(RPC_WALLET_ERROR,
                                    "Error adding address to wallet");
            }

            // add to address book or update label
            if (IsValidDestination(pubkey_dest)) {
                pwallet->SetAddressBook(pubkey_dest, label, "receive");
            }

            // TODO Is this necessary?
            CScript scriptRawPubKey = GetScriptForRawPubKey(pubKey);

            if (::IsMine(*pwallet, scriptRawPubKey) == ISMINE_SPENDABLE) {
                throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already "
                                                        "contains the private "
                                                        "key for this address "
                                                        "or script");
            }

            pwallet->MarkDirty();

            if (!pwallet->AddWatchOnly(scriptRawPubKey, timestamp)) {
                throw JSONRPCError(RPC_WALLET_ERROR,
                                    "Error adding address to wallet");
            }

            success = true;
    

            // Import private keys.
            if (keys.size()) {
                const std::string &strPrivkey = keys[0].get_str();

                // Checks.
                CKey key = DecodeSecret(strPrivkey);

                if (!key.IsValid()) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                                       "Invalid private key encoding");
                }

                CPubKey pubKey = key.GetPubKey();
                assert(key.VerifyPubKey(pubKey));

                CTxDestination pubkey_dest = pubKey.GetID();

                // Consistency check.
                if (!(pubkey_dest == dest)) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                                       "Consistency check failed");
                }

                CKeyID vchAddress = pubKey.GetID();
                pwallet->MarkDirty();
                pwallet->SetAddressBook(vchAddress, label, "receive");

                if (pwallet->HaveKey(vchAddress)) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already "
                                                         "contains the private "
                                                         "key for this address "
                                                         "or script");
                }

                pwallet->mapKeyMetadata[vchAddress].nCreateTime = timestamp;

                if (!pwallet->AddKeyPubKey(key, pubKey)) {
                    throw JSONRPCError(RPC_WALLET_ERROR,
                                       "Error adding key to wallet");
                }

                pwallet->UpdateTimeFirstKey(timestamp);

                success = true;
            }

            // Import scriptPubKey only.
            if (pubKeys.size() == 0 && keys.size() == 0) {
                if (::IsMine(*pwallet, script) == ISMINE_SPENDABLE) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already "
                                                         "contains the private "
                                                         "key for this address "
                                                         "or script");
                }

                pwallet->MarkDirty();

                if (!pwallet->AddWatchOnly(script, timestamp)) {
                    throw JSONRPCError(RPC_WALLET_ERROR,
                                       "Error adding address to wallet");
                }

                if (!internal) {
                    // add to address book or update label
                    if (IsValidDestination(dest)) {
                        pwallet->SetAddressBook(dest, label, "receive");
                    }
                }

                success = true;
            }
        }

        UniValue::Object result;
        result.reserve(1);
        result.emplace_back("success", success);
        return result;
    } catch (JSONRPCError &e) {
        UniValue::Object result;
        result.reserve(2);
        result.emplace_back("success", false);
        result.emplace_back("error", std::move(e).toObj());
        return result;
    } catch (...) {
        UniValue::Object result;
        result.reserve(2);
        result.emplace_back("success", false);
        result.emplace_back("error", JSONRPCError(RPC_MISC_ERROR, "Missing required fields").toObj());
        return result;
    }
}

static int64_t GetImportTimestamp(const UniValue &data, int64_t now) {
    if (auto timestampUV = data.locate("timestamp")) {
        const UniValue &timestamp = *timestampUV;
        if (timestamp.isNum()) {
            return timestamp.get_int64();
        } else if (timestamp.isStr() && timestamp.get_str() == "now") {
            return now;
        }
        throw JSONRPCError(RPC_TYPE_ERROR,
                           strprintf("Expected number or \"now\" timestamp "
                                     "value for key. got type %s",
                                     uvTypeName(timestamp.type())));
    }
    throw JSONRPCError(RPC_TYPE_ERROR,
                       "Missing required timestamp field for key");
}

UniValue importmulti(const Config &, const JSONRPCRequest &mainRequest) {
    std::shared_ptr<CWallet> const wallet =
        GetWalletForJSONRPCRequest(mainRequest);
    CWallet *const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, mainRequest.fHelp)) {
        return UniValue();
    }

    // clang-format off
    if (mainRequest.fHelp || mainRequest.params.size() < 1 || mainRequest.params.size() > 2) {
        throw std::runtime_error(
            RPCHelpMan{"importmulti",
                "\nImport addresses/scripts (with private or public keys), rescanning all addresses in one-shot-only (rescan can be disabled via options). Requires a new wallet backup.\n",
                {
                    {"requests", RPCArg::Type::ARR, /* opt */ false, /* default_val */ "", "Data to be imported",
                        {
                            {"", RPCArg::Type::OBJ, /* opt */ false, /* default_val */ "", "",
                                {
                                    {"scriptPubKey", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", "Type of scriptPubKey (string for script, json for address)",
                                        /* oneline_description */ "", {"\"<script>\" | { \"address\":\"<address>\" }", "string / json"}
                                    },
                                    {"timestamp", RPCArg::Type::NUM, /* opt */ false, /* default_val */ "", "Creation time of the key in seconds since epoch (Jan 1 1970 GMT),\n"
        "                                                              or the string \"now\" to substitute the current synced blockchain time. The timestamp of the oldest\n"
        "                                                              key will determine how far back blockchain rescans need to begin for missing wallet transactions.\n"
        "                                                              \"now\" can be specified to bypass scanning, for keys which are known to never have been used, and\n"
        "                                                              0 can be specified to scan the entire blockchain. Blocks up to 2 hours before the earliest key\n"
        "                                                              creation time of all keys being imported by the importmulti call will be scanned.",
                                        /* oneline_description */ "", {"timestamp | \"now\"", "integer / string"}
                                    },
                                    {"pubkeys", RPCArg::Type::ARR, /* opt */ true, /* default_val */ "", "Array of strings giving pubkeys that must occur in the output or redeemscript",
                                        {
                                            {"pubKey", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", ""},
                                        }
                                    },
                                    {"keys", RPCArg::Type::ARR, /* opt */ true, /* default_val */ "", "Array of strings giving private keys whose corresponding public keys must occur in the output or redeemscript",
                                        {
                                            {"key", RPCArg::Type::STR, /* opt */ false, /* default_val */ "", ""},
                                        }
                                    },
                                    {"internal", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "false", "Stating whether matching outputs should be treated as not incoming payments aka change"},
                                    {"watchonly", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "false", "Stating whether matching outputs should be considered watched even when they're not spendable, only allowed if keys are empty"},
                                    {"label", RPCArg::Type::STR, /* opt */ true, /* default_val */ "''", "Label to assign to the address, only allowed with internal=false"},
                                },
                            },
                        },
                        "\"requests\""},
                    {"options", RPCArg::Type::OBJ, /* opt */ true, /* default_val */ "", "",
                        {
                            {"rescan", RPCArg::Type::BOOL, /* opt */ true, /* default_val */ "true", "Stating if should rescan the blockchain after all imports"},
                        },
                        "\"options\""},
                }}
                .ToString() +
            "\nNote: This call can take over an hour to complete if rescan is true, during that time, other rpc calls\n"
            "may report that the imported keys, addresses or scripts exists but related transactions are still missing.\n"
            "\nExamples:\n" +
            HelpExampleCli("importmulti", "'[{ \"scriptPubKey\": { \"address\": \"<my address>\" }, \"timestamp\":1455191478 }, "
                                          "{ \"scriptPubKey\": { \"address\": \"<my 2nd address>\" }, \"label\": \"example 2\", \"timestamp\": 1455191480 }]'") +
            HelpExampleCli("importmulti", "'[{ \"scriptPubKey\": { \"address\": \"<my address>\" }, \"timestamp\":1455191478 }]' '{ \"rescan\": false}'") +

            "\nResponse is an array with the same size as the input that has the execution result:\n"
            "  [{ \"success\": true } , { \"success\": false, \"error\": { \"code\": -1, \"message\": \"Internal Server Error\"} }, ... ]\n");
    }


    RPCTypeCheck(mainRequest.params, {UniValue::VARR, UniValue::VOBJ});

    const UniValue::Array &requests = mainRequest.params[0].get_array();

    // Default options
    bool fRescan = true;

    if (!mainRequest.params[1].isNull()) {
        const UniValue &options = mainRequest.params[1];

        if (auto rescanUV = options.locate("rescan")) {
            fRescan = rescanUV->get_bool();
        }
    }

    WalletRescanReserver reserver(pwallet);
    if (fRescan && !reserver.reserve()) {
        throw JSONRPCError(
            RPC_WALLET_ERROR,
            "Wallet is currently rescanning. Abort existing rescan or wait.");
    }

    int64_t now = 0;
    bool fRunScan = false;
    int64_t nLowestTimestamp = 0;
    UniValue::Array response;
    {
        auto locked_chain = pwallet->chain().lock();
        LOCK(pwallet->cs_wallet);
        EnsureWalletIsUnlocked(pwallet);

        // Verify all timestamps are present before importing any keys.
        const std::optional<int> tip_height = locked_chain->getHeight();
        now = tip_height ? locked_chain->getBlockMedianTimePast(*tip_height) : 0;
        for (auto &data : requests) {
            GetImportTimestamp(data, now);
        }

        const int64_t minimumTimestamp = 1;

        if (fRescan && tip_height) {
            nLowestTimestamp = locked_chain->getBlockTime(*tip_height);
        } else {
            fRescan = false;
        }

        response.reserve(requests.size());

        for (const UniValue &data : requests) {
            const int64_t timestamp =  std::max(GetImportTimestamp(data, now), minimumTimestamp);
            response.push_back( ProcessImport(pwallet, data, timestamp) );

            if (!fRescan) {
                continue;
            }

            const UniValue & result = response.back();

            // If at least one request was successful then allow rescan.
            if (result["success"].get_bool()) {
                fRunScan = true;
            }

            // Get the lowest timestamp.
            if (timestamp < nLowestTimestamp) {
                nLowestTimestamp = timestamp;
            }
        }
    }
    if (fRescan && fRunScan && requests.size()) {
        int64_t scannedTime = pwallet->RescanFromTime(
            nLowestTimestamp, reserver, true /* update */);
        pwallet->ReacceptWalletTransactions();

        if (pwallet->IsAbortingRescan()) {
            throw JSONRPCError(RPC_MISC_ERROR, "Rescan aborted by user.");
        }
        if (scannedTime > nLowestTimestamp) {
            UniValue::Array results = std::move(response); // cheap constant-time move
            // response is now an empty array after the above call
            response.reserve(requests.size());
            assert(results.size() == requests.size());
            size_t i = 0;
            for (const UniValue &request : requests) {
                // If key creation date is within the successfully scanned
                // range, or if the import result already has an error set, let
                // the result stand unmodified. Otherwise replace the result
                // with an error message.
                if (scannedTime <= GetImportTimestamp(request, now) ||
                    results[i].locate("error")) {
                    response.push_back(std::move(results[i]));
                } else {
                    UniValue::Object result;
                    result.reserve(2);
                    result.emplace_back("success", false);
                    result.emplace_back(
                        "error",
                        JSONRPCError(
                            RPC_MISC_ERROR,
                            strprintf(
                                "Rescan failed for key with creation timestamp "
                                "%d. There was an error reading a block from "
                                "time %d, which is after or within %d seconds "
                                "of key creation, and could contain "
                                "transactions pertaining to the key. As a "
                                "result, transactions and coins using this key "
                                "may not appear in the wallet. This error "
                                "could be caused by pruning or data corruption "
                                "(see bitcoind log for details) and could be "
                                "dealt with by downloading and rescanning the "
                                "relevant blocks (see -reindex and -rescan "
                                "options).",
                                GetImportTimestamp(request, now),
                                scannedTime - TIMESTAMP_WINDOW - 1,
                                TIMESTAMP_WINDOW)).toObj());
                    response.emplace_back(std::move(result));
                }
                ++i;
            }
        }
    }

    return response;
}

// clang-format off
static const ContextFreeRPCCommand commands[] = {
    //  category            name                        actor (function)          argNames
    //  ------------------- ------------------------    ----------------------    ----------
    { "wallet",             "abortrescan",              abortrescan,              {} },
    { "wallet",             "dumpprivkey",              dumpprivkey,              {"address"}  },
    { "wallet",             "dumpwallet",               dumpwallet,               {"filename"} },
    { "wallet",             "importmulti",              importmulti,              {"requests","options"} },
    { "wallet",             "importprivkey",            importprivkey,            {"privkey","label","rescan"} },
    { "wallet",             "importwallet",             importwallet,             {"filename"} },
    { "wallet",             "importaddress",            importaddress,            {"address","label","rescan"} },
    { "wallet",             "importprunedfunds",        importprunedfunds,        {"rawtransaction","txoutproof"} },
    { "wallet",             "importpubkey",             importpubkey,             {"pubkey","label","rescan"} },
    { "wallet",             "removeprunedfunds",        removeprunedfunds,        {"txid"} },
};
// clang-format on

void RegisterDumpRPCCommands(CRPCTable &t) {
    for (unsigned int vcidx = 0; vcidx < std::size(commands); ++vcidx) {
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
    }
}
