// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <config.h>
#include <init.h>
#include <interfaces/chain.h>
#include <net.h>
#include <scheduler.h>
#include <util/moneystr.h>
#include <util/system.h>
#include <validation.h>
#include <wallet/rpcdump.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>
#include <wallet/walletutil.h>
#include <walletinitinterface.h>

class WalletInit : public WalletInitInterface {
public:
    //! Was the wallet component compiled in.
    bool HasWalletSupport() const override { return true; }

    //! Return the wallets help message.
    void AddWalletOptions() const override;

    //! Wallets parameter interaction
    bool ParameterInteraction() const override;

    //! Add wallets that should be opened to list of chain clients.
    void Construct(NodeContext &node) const override;
};

const WalletInitInterface &g_wallet_init_interface = WalletInit();

void WalletInit::AddWalletOptions() const {
    gArgs.AddArg(
        "-avoidpartialspends",
        strprintf("Group outputs by address, selecting all or none, instead of "
                  "selecting on a per-output basis. Privacy is improved as an "
                  "address is only used once (unless someone sends to it after "
                  "spending from it), but may result in slightly higher fees "
                  "as suboptimal coin selection may result due to the added "
                  "limitation (default: %u)",
                  DEFAULT_AVOIDPARTIALSPENDS),
        ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);

    gArgs.AddArg("-disablewallet",
                 "Do not load the wallet and disable wallet RPC calls", ArgsManager::ALLOW_ANY,
                 OptionsCategory::WALLET);
    gArgs.AddArg("-fallbackfee=<amt>",
                 strprintf("A fee rate (in %s/kB) that will be used when fee "
                           "estimation has insufficient data (default: %s)",
                           CURRENCY_UNIT, FormatMoney(DEFAULT_FALLBACK_FEE)),
                 ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    gArgs.AddArg("-keypool=<n>",
                 strprintf("Set key pool size to <n> (default: %u)",
                           DEFAULT_KEYPOOL_SIZE),
                 ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    gArgs.AddArg(
        "-maxtxfee=<amt>",
        strprintf("Maximum total fees (in %s) to use in a single wallet "
                  "transaction or raw transaction; setting this too low may "
                  "abort large transactions (default: %s)",
                  CURRENCY_UNIT, FormatMoney(DEFAULT_TRANSACTION_MAXFEE)),
        ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-mintxfee=<amt>",
                 strprintf("Fees (in %s/kB) smaller than this are considered "
                           "zero fee for transaction creation (default: %s)",
                           CURRENCY_UNIT,
                           FormatMoney(DEFAULT_TRANSACTION_MINFEE_PER_KB)),
                 ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    gArgs.AddArg(
        "-paytxfee=<amt>",
        strprintf(
            "Fee (in %s/kB) to add to transactions you send (default: %s)",
            CURRENCY_UNIT,
            FormatMoney(CFeeRate{DEFAULT_PAY_TX_FEE}.GetFeePerK())),
        ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    gArgs.AddArg(
        "-rescan",
        "Rescan the block chain for missing wallet transactions on startup",
        ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    gArgs.AddArg(
        "-salvagewallet",
        "Attempt to recover private keys from a corrupt wallet on startup",
        ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);

    gArgs.AddArg(
        "-spendzeroconfchange",
        strprintf(
            "Spend unconfirmed change when sending transactions (default: %d)",
            DEFAULT_SPEND_ZEROCONF_CHANGE),
        ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    gArgs.AddArg("-usebip69",
                 strprintf("Lexicographically sort transaction inputs and "
                           "outputs as defined in BIP69 (default: %d)",
                           DEFAULT_USE_BIP69),
                 ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    gArgs.AddArg("-upgradewallet", "Upgrade wallet to latest format on startup",
                 ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    gArgs.AddArg("-wallet=<path>",
                 "Specify wallet database path. Can be specified multiple "
                 "times to load multiple wallets. Path is interpreted relative "
                 "to <walletdir> if it is not absolute, and will be created if "
                 "it does not exist (as a directory containing a wallet.dat "
                 "file and log files). For backwards compatibility this will "
                 "also accept names of existing data files in <walletdir>.)",
                 ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::WALLET);
    gArgs.AddArg(
        "-walletbroadcast",
        strprintf("Make the wallet broadcast transactions (default: %d)",
                  DEFAULT_WALLETBROADCAST),
        ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    gArgs.AddArg("-walletdir=<dir>",
                 "Specify directory to hold wallets (default: "
                 "<datadir>/wallets if it exists, otherwise <datadir>)",
                 ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    gArgs.AddArg("-walletnotify=<cmd>",
                 "Execute command when a wallet transaction changes (%s in cmd "
                 "is replaced by TxID)",
                 ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);
    gArgs.AddArg(
        "-zapwallettxes=<mode>",
        "Delete all wallet transactions and only recover those parts of the "
        "blockchain through -rescan on startup (1 = keep tx meta data e.g. "
        "payment request information, 2 = drop tx meta data)",
        ArgsManager::ALLOW_ANY, OptionsCategory::WALLET);

    gArgs.AddArg("-dblogsize=<n>",
                 strprintf("Flush wallet database activity from memory to disk "
                           "log every <n> megabytes (default: %u)",
                           DEFAULT_WALLET_DBLOGSIZE),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::WALLET_DEBUG_TEST);
    gArgs.AddArg(
        "-flushwallet",
        strprintf("Run a thread to flush wallet periodically (default: %d)",
                  DEFAULT_FLUSHWALLET),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::WALLET_DEBUG_TEST);
    gArgs.AddArg("-privdb",
                 strprintf("Sets the DB_PRIVATE flag in the wallet db "
                           "environment (default: %d)",
                           DEFAULT_WALLET_PRIVDB),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::WALLET_DEBUG_TEST);
}

bool WalletInit::ParameterInteraction() const {
    const bool is_multiwallet = gArgs.GetArgs("-wallet").size() > 1;

    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        return true;
    }

    if (gArgs.GetBoolArg("-blocksonly", DEFAULT_BLOCKSONLY) &&
        gArgs.SoftSetBoolArg("-walletbroadcast", false)) {
        LogPrintf("%s: parameter interaction: -blocksonly=1 -> setting "
                  "-walletbroadcast=0\n",
                  __func__);
    }

    if (gArgs.GetBoolArg("-salvagewallet", false)) {
        if (is_multiwallet) {
            return InitError(
                strprintf("%s is only allowed with a single wallet file",
                          "-salvagewallet"));
        }
        // Rewrite just private keys: rescan to find transactions
        if (gArgs.SoftSetBoolArg("-rescan", true)) {
            LogPrintf("%s: parameter interaction: -salvagewallet=1 -> setting "
                      "-rescan=1\n",
                      __func__);
        }
    }

    bool zapwallettxes = gArgs.GetBoolArg("-zapwallettxes", false);
    // -zapwallettxes implies dropping the mempool on startup
    if (zapwallettxes && gArgs.SoftSetBoolArg("-persistmempool", false)) {
        LogPrintf("%s: parameter interaction: -zapwallettxes enabled -> "
                  "setting -persistmempool=0\n",
                  __func__);
    }

    // -zapwallettxes implies a rescan
    if (zapwallettxes) {
        if (is_multiwallet) {
            return InitError(
                strprintf("%s is only allowed with a single wallet file",
                          "-zapwallettxes"));
        }
        if (gArgs.SoftSetBoolArg("-rescan", true)) {
            LogPrintf("%s: parameter interaction: -zapwallettxes enabled -> "
                      "setting -rescan=1\n",
                      __func__);
        }
    }

    if (is_multiwallet) {
        if (gArgs.GetBoolArg("-upgradewallet", false)) {
            return InitError(
                strprintf("%s is only allowed with a single wallet file",
                          "-upgradewallet"));
        }
    }

    if (gArgs.GetBoolArg("-sysperms", false)) {
        return InitError("-sysperms is not allowed in combination with enabled "
                         "wallet functionality");
    }

    if (gArgs.GetArg("-prune", 0) && gArgs.GetBoolArg("-rescan", false)) {
        return InitError(
            _("Rescans are not possible in pruned mode. You will need to use "
              "-reindex which will download the whole blockchain again."));
    }

    if (minRelayTxFee.GetFeePerK() > HIGH_TX_FEE_PER_KB) {
        InitWarning(
            AmountHighWarn("-minrelaytxfee") + " " +
            _("The wallet will avoid paying less than the minimum relay fee."));
    }

    if (gArgs.IsArgSet("-maxtxfee")) {
        Amount nMaxFee = Amount::zero();
        if (!ParseMoney(gArgs.GetArg("-maxtxfee", ""), nMaxFee)) {
            return InitError(
                AmountErrMsg("maxtxfee", gArgs.GetArg("-maxtxfee", "")));
        }

        if (nMaxFee > HIGH_MAX_TX_FEE) {
            InitWarning(_("-maxtxfee is set very high! Fees this large could "
                          "be paid on a single transaction."));
        }

        maxTxFee = nMaxFee;
        if (CFeeRate(maxTxFee, 1000) < minRelayTxFee) {
            return InitError(strprintf(
                _("Invalid amount for -maxtxfee=<amount>: '%s' (must "
                  "be at least the minrelay fee of %s to prevent "
                  "stuck transactions)"),
                gArgs.GetArg("-maxtxfee", ""), minRelayTxFee.ToString()));
        }
    }

    return true;
}

bool VerifyWallets(const CChainParams &chainParams, interfaces::Chain &chain,
                   const std::vector<std::string> &wallet_files) {
    if (gArgs.IsArgSet("-walletdir")) {
        fs::path wallet_dir = gArgs.GetArg("-walletdir", "");
        boost::system::error_code error;
        // The canonical path cleans the path, preventing >1 Berkeley
        // environment instances for the same directory
        fs::path canonical_wallet_dir = fs::canonical(wallet_dir, error);
        if (error || !fs::exists(wallet_dir)) {
            return InitError(
                strprintf(_("Specified -walletdir \"%s\" does not exist"),
                          wallet_dir.string()));
        } else if (!fs::is_directory(wallet_dir)) {
            return InitError(
                strprintf(_("Specified -walletdir \"%s\" is not a directory"),
                          wallet_dir.string()));
            // The canonical path transforms relative paths into absolute ones,
            // so we check the non-canonical version
        } else if (!wallet_dir.is_absolute()) {
            return InitError(
                strprintf(_("Specified -walletdir \"%s\" is a relative path"),
                          wallet_dir.string()));
        }
        gArgs.ForceSetArg("-walletdir", canonical_wallet_dir.string());
    }

    LogPrintf("Using wallet directory %s\n", GetWalletDir().string());

    uiInterface.InitMessage(wallet_files.size() == 1 ? _("Verifying wallet...") : _("Verifying wallets..."));

    // Parameter interaction code should have thrown an error if -salvagewallet
    // was enabled with more than wallet file, so the wallet_files size check
    // here should have no effect.
    bool salvage_wallet =
        gArgs.GetBoolArg("-salvagewallet", false) && wallet_files.size() <= 1;

    // Keep track of each wallet absolute path to detect duplicates.
    std::set<fs::path> wallet_paths;

    for (const auto &wallet_file : wallet_files) {
        WalletLocation location(wallet_file);

        if (!wallet_paths.insert(location.GetPath()).second) {
            return InitError(strprintf(_("Error loading wallet %s. Duplicate "
                                         "-wallet filename specified."),
                                       wallet_file));
        }

        std::string error_string;
        std::string warning_string;
        bool verify_success =
            CWallet::Verify(chainParams, chain, location, salvage_wallet,
                            error_string, warning_string);
        if (!error_string.empty()) {
            InitError(error_string);
        }
        if (!warning_string.empty()) {
            InitWarning(warning_string);
        }
        if (!verify_success) {
            return false;
        }
    }

    return true;
}

void WalletInit::Construct(NodeContext &node) const {
    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        LogPrintf("Wallet disabled!\n");
        return;
    }
    gArgs.SoftSetArg("-wallet", "");
    node.chain_clients.emplace_back(
        interfaces::MakeWalletClient(*node.chain, gArgs.GetArgs("-wallet")));
}

bool LoadWallets(const CChainParams &chainParams, interfaces::Chain &chain,
                 const std::vector<std::string> &wallet_files) {
    for (const std::string &walletFile : wallet_files) {
        std::shared_ptr<CWallet> pwallet = CWallet::CreateWalletFromFile(
            chainParams, chain, WalletLocation(walletFile));
        if (!pwallet) {
            return false;
        }
        AddWallet(pwallet);
    }

    return true;
}

void StartWallets(CScheduler &scheduler) {
    for (const std::shared_ptr<CWallet> &pwallet : GetWallets()) {
        pwallet->postInitProcess();
    }

    // Run a thread to flush wallet periodically
    scheduler.scheduleEvery(
        [] {
            MaybeCompactWalletDB();
            return true;
        },
        500);
}

void FlushWallets() {
    for (const std::shared_ptr<CWallet> &pwallet : GetWallets()) {
        pwallet->Flush(false);
    }
}

void StopWallets() {
    for (const std::shared_ptr<CWallet> &pwallet : GetWallets()) {
        pwallet->Flush(true);
    }
}

void UnloadWallets() {
    auto wallets = GetWallets();
    while (!wallets.empty()) {
        auto wallet = wallets.back();
        wallets.pop_back();
        RemoveWallet(wallet);
        UnloadWallet(std::move(wallet));
    }
}
