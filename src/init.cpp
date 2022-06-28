// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <init.h>

#include <addrman.h>
#include <amount.h>
#include <banman.h>
#include <chain.h>
#include <chainparams.h>
#include <checkpoints.h>
#include <compat/sanity.h>
#include <config.h>
#include <consensus/validation.h>
#include <dsproof/dsproof.h>
#include <dsproof/storage.h>
#include <extversion.h>
#include <flatfile.h>
#include <fs.h>
#include <gbtlight.h>
#include <hash.h>
#include <httprpc.h>
#include <httpserver.h>
#include <index/txindex.h>
#include <interfaces/chain.h>
#include <key.h>
#include <miner.h>
#include <net.h>
#include <net_permissions.h>
#include <net_processing.h>
#include <netbase.h>
#include <policy/mempool.h>
#include <policy/policy.h>
#include <rpc/blockchain.h>
#include <rpc/mining.h>
#include <rpc/register.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <scheduler.h>
#include <script/scriptcache.h>
#include <script/sigcache.h>
#include <script/standard.h>
#include <shutdown.h>
#include <software_outdated.h>
#include <timedata.h>
#include <torcontrol.h>
#include <txdb.h>
#include <txmempool.h>
#include <ui_interface.h>
#include <util/asmap.h>
#include <util/moneystr.h>
#include <util/string.h>
#include <util/system.h>
#include <util/threadnames.h>
#include <validation.h>
#include <validationinterface.h>
#include <walletinitinterface.h>
#include <warnings.h>

#include <boost/thread.hpp>

#if ENABLE_ZMQ
#include <zmq/zmqnotificationinterface.h>
#include <zmq/zmqrpc.h>
#endif

#ifndef WIN32
#include <attributes.h>
#include <cerrno>
#include <csignal>
#include <sys/stat.h>
#endif
#include <cstdint>
#include <cstdio>
#include <memory>

#ifdef ENABLE_WALLET
#include <db_cxx.h> // DbEnv::version
#endif

/** Default for -proxyrandomize */
static constexpr bool DEFAULT_PROXYRANDOMIZE = true;
/** Default for -rest */
static constexpr bool DEFAULT_REST_ENABLE = false;
/** Default for -stopafterblockimport */
static constexpr bool DEFAULT_STOPAFTERBLOCKIMPORT = false;

// Dump addresses to banlist.dat every 15 minutes (900s)
static constexpr int DUMP_BANS_INTERVAL = 60 * 15;

std::unique_ptr<CConnman> g_connman;
std::unique_ptr<PeerLogicValidation> peerLogic;
std::unique_ptr<BanMan> g_banman;

#ifdef WIN32
// Win32 LevelDB doesn't use filedescriptors, and the ones used for accessing
// block files don't count towards the fd_set size limit anyway.
#define MIN_CORE_FILEDESCRIPTORS 0
#else
#define MIN_CORE_FILEDESCRIPTORS 150
#endif

static const char *const DEFAULT_ASMAP_FILENAME = "ip_asn.map";

/**
 * The PID file facilities.
 */
static const char *BITCOIN_PID_FILENAME = "bitcoind.pid";

static fs::path GetPidFile() {
    return AbsPathForConfigVal(
        fs::path(gArgs.GetArg("-pid", BITCOIN_PID_FILENAME)));
}

[[nodiscard]] static bool CreatePidFile() {
    FILE *file = fsbridge::fopen(GetPidFile(), "w");
    if (file) {
#ifdef WIN32
        std::fprintf(file, "%lu\n", static_cast<unsigned long>(GetCurrentProcessId()));
#else
        std::fprintf(file, "%ld\n", static_cast<long>(getpid()));
#endif
        std::fclose(file);
        return true;
    } else {
        return InitError(strprintf(_("Unable to create the PID file '%s': %s"),
                                   GetPidFile().string(),
                                   std::strerror(errno)));
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown
//

//
// Thread management and startup/shutdown:
//
// The network-processing threads are all part of a thread group created by
// AppInit() or the Qt main() function.
//
// A clean exit happens when StartShutdown() or the SIGTERM signal handler sets
// ShutdownRequested(), which triggers the DetectShutdownThread(), which
// interrupts the main thread group. DetectShutdownThread() then exits, which
// causes AppInit() to continue (it .joins the shutdown thread). Shutdown() is
// then called to clean up database connections, and stop other threads that
// should only be stopped after the main network-processing threads have exited.
//
// Shutdown for Qt is very similar, only it uses a QTimer to detect
// ShutdownRequested() getting set, and then does the normal Qt shutdown thing.
//

/**
 * This is a minimally invasive approach to shutdown on LevelDB read errors from
 * the chainstate, while keeping user interface out of the common library, which
 * is shared between bitcoind, and bitcoin-qt and non-server tools.
 */
class CCoinsViewErrorCatcher final : public CCoinsViewBacked {
public:
    explicit CCoinsViewErrorCatcher(CCoinsView *view)
        : CCoinsViewBacked(view) {}
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override {
        try {
            return CCoinsViewBacked::GetCoin(outpoint, coin);
        } catch (const std::runtime_error &e) {
            uiInterface.ThreadSafeMessageBox(
                _("Error reading from database, shutting down."), "",
                CClientUIInterface::MSG_ERROR);
            LogPrintf("Error reading from database: %s\n", e.what());
            // Starting the shutdown sequence and returning false to the caller
            // would be interpreted as 'entry not found' (as opposed to unable
            // to read data), and could lead to invalid interpretation. Just
            // exit immediately, as we can't continue anyway, and all writes
            // should be atomic.
            abort();
        }
    }
    // Writes do not need similar protection, as failure to write is handled by
    // the caller.
};

static std::unique_ptr<CCoinsViewErrorCatcher> pcoinscatcher;
static std::unique_ptr<ECCVerifyHandle> globalVerifyHandle;

static boost::thread_group threadGroup;
static CScheduler scheduler;

void Interrupt() {
    InterruptHTTPServer();
    InterruptHTTPRPC();
    InterruptRPC();
    InterruptREST();
    InterruptTorControl();
    InterruptMapPort();
    if (g_connman) {
        g_connman->Interrupt();
    }
    if (g_txindex) {
        g_txindex->Interrupt();
    }
}

void Shutdown(NodeContext &node) {
    LogPrintf("%s: In progress...\n", __func__);
    static RecursiveMutex cs_Shutdown;
    TRY_LOCK(cs_Shutdown, lockShutdown);
    if (!lockShutdown) {
        return;
    }

    /// Note: Shutdown() must be able to handle cases in which initialization failed part of the way,
    /// for example if the data directory was found to be locked.
    /// Be sure that anything that writes files or flushes caches only does this if the respective
    /// module was initialized.
    util::ThreadRename("shutoff");
    g_mempool.AddTransactionsUpdated(1);

    StopHTTPRPC();
    StopREST();
    StopRPC();
    StopHTTPServer();
    for (const auto &client : node.chain_clients) {
        client->flush();
    }
    StopMapPort();

    // Because these depend on each-other, we make sure that neither can be
    // using the other before destroying them.
    if (peerLogic) {
        UnregisterValidationInterface(peerLogic.get());
    }
    if (g_connman) {
        g_connman->Stop();
    }
    if (g_txindex) {
        g_txindex->Stop();
    }

    StopTorControl();

    // After everything has been shut down, but before things get flushed, stop
    // the CScheduler/checkqueue threadGroup
    threadGroup.interrupt_all();
    threadGroup.join_all();
    StopScriptCheckWorkerThreads();

    // After the threads that potentially access these pointers have been
    // stopped, destruct and reset all to nullptr.
    peerLogic.reset();
    g_connman.reset();
    g_banman.reset();
    g_txindex.reset();

    if (::g_mempool.IsLoaded() &&
        gArgs.GetArg("-persistmempool", DEFAULT_PERSIST_MEMPOOL)) {
        DumpMempool(::g_mempool);
        if (DoubleSpendProof::IsEnabled()) {
            DumpDSProofs(::g_mempool);
        }
    }

    // FlushStateToDisk generates a ChainStateFlushed callback, which we should
    // avoid missing
    if (pcoinsTip != nullptr) {
        FlushStateToDisk();
    }

    // After there are no more peers/RPC left to give us new data which may
    // generate CValidationInterface callbacks, flush them...
    GetMainSignals().FlushBackgroundCallbacks();

    // Any future callbacks will be dropped. This should absolutely be safe - if
    // missing a callback results in an unrecoverable situation, unclean
    // shutdown would too. The only reason to do the above flushes is to let the
    // wallet catch up with our current chain to avoid any strange pruning edge
    // cases and make next startup faster by avoiding rescan.

    {
        LOCK(cs_main);
        if (pcoinsTip != nullptr) {
            FlushStateToDisk();
        }
        pcoinsTip.reset();
        pcoinscatcher.reset();
        pcoinsdbview.reset();
        pblocktree.reset();
    }
    for (const auto &client : node.chain_clients) {
        client->stop();
    }

#if ENABLE_ZMQ
    if (g_zmq_notification_interface) {
        UnregisterValidationInterface(g_zmq_notification_interface);
        delete g_zmq_notification_interface;
        g_zmq_notification_interface = nullptr;
    }
#endif

    try {
        if (!fs::remove(GetPidFile())) {
            LogPrintf("%s: Unable to remove PID file: File does not exist\n",
                      __func__);
        }
    } catch (const fs::filesystem_error &e) {
        LogPrintf("%s: Unable to remove PID file: %s\n", __func__,
                  fsbridge::get_filesystem_error_message(e));
    }
    node.chain_clients.clear();
    rpc::UnregisterSubmitBlockCatcher();
    UnregisterAllValidationInterfaces();
    GetMainSignals().UnregisterBackgroundSignalScheduler();
    GetMainSignals().UnregisterWithMempoolSignals(g_mempool);
    globalVerifyHandle.reset();
    ECC_Stop();
    LogPrintf("%s: done\n", __func__);
}

/**
 * Signal handlers are very limited in what they are allowed to do.
 * The execution context the handler is invoked in is not guaranteed,
 * so we restrict handler operations to just touching variables:
 */
#ifndef WIN32
static void HandleSIGTERM(int) {
    StartShutdown();
}

static void HandleSIGHUP(int) {
    LogInstance().m_reopen_file = true;
}
#else
static BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType) {
    StartShutdown();
    Sleep(INFINITE);
    return true;
}
#endif

#ifndef WIN32
static void registerSignalHandler(int signal, void (*handler)(int)) {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, NULL);
}
#endif

static void OnRPCStarted() {
    uiInterface.NotifyBlockTip_connect(&RPCNotifyBlockChange);
}

static void OnRPCStopped() {
    uiInterface.NotifyBlockTip_disconnect(&RPCNotifyBlockChange);
    RPCNotifyBlockChange(false, nullptr);
    g_best_block_cv.notify_all();
    LogPrint(BCLog::RPC, "RPC stopped.\n");
}

void SetupServerArgs() {
    SetupHelpOptions(gArgs);
    gArgs.AddArg("-??, -hh, -help-debug", "Print this help message including advanced debugging options and exit",
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);

    const auto defaultBaseParams = CreateBaseChainParams(CBaseChainParams::MAIN);
    const auto testnetBaseParams = CreateBaseChainParams(CBaseChainParams::TESTNET);
    const auto testnet4BaseParams = CreateBaseChainParams(CBaseChainParams::TESTNET4);
    const auto regtestBaseParams = CreateBaseChainParams(CBaseChainParams::REGTEST);
    const auto scalenetBaseParams = CreateBaseChainParams(CBaseChainParams::SCALENET);
    const auto defaultChainParams = CreateChainParams(CBaseChainParams::MAIN);
    const auto testnetChainParams = CreateChainParams(CBaseChainParams::TESTNET);
    const auto testnet4ChainParams = CreateChainParams(CBaseChainParams::TESTNET4);
    const auto regtestChainParams = CreateChainParams(CBaseChainParams::REGTEST);
    const auto scalenetChainParams = CreateChainParams(CBaseChainParams::SCALENET);

    // Hidden Options
    std::vector<std::string> hidden_args = {
        "-dbcrashratio", "-forcecompactdb", "-expirerpc",
        // GUI args. These will be overwritten by SetupUIArgs for the GUI
        "-allowselfsignedrootcertificates", "-choosedatadir", "-lang=<lang>",
        "-min", "-resetguisettings", "-rootcertificates=<file>", "-splash",
        "-uiplatform"};

    // Set all of the args and their help
    // When adding new options to the categories, please keep and ensure alphabetical ordering.
    // Do not translate _(...) any options as decided in D4515/PR13341.
    gArgs.AddArg("-version", "Print version and exit", ArgsManager::ALLOW_ANY,
                 OptionsCategory::OPTIONS);
    gArgs.AddArg("-alertnotify=<cmd>",
                 "Execute command when a relevant alert is received or we see "
                 "a really long fork (%s in cmd is replaced by message)",
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg(
        "-assumevalid=<hex>",
        strprintf(
            "If this block is in the chain assume that it and its ancestors "
            "are valid and potentially skip their script verification (0 to "
            "verify all, default: %s, testnet: %s)",
            defaultChainParams->GetConsensus().defaultAssumeValid.GetHex(),
            testnetChainParams->GetConsensus().defaultAssumeValid.GetHex()),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-automaticunparking",
                 strprintf("If a new block is connected to a parked chain "
                           "with now much more proof-of-work than the active "
                           "chain, then unpark the parked chain automatically "
                           "(default: %d)",
                           DEFAULT_AUTOMATIC_UNPARKING),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-blocksdir=<dir>",
                 "Specify directory to hold blocks subdirectory for *.dat "
                 "files (default: <datadir>)",
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-indexdir=<dir>",
                 "Specify directory to hold leveldb files (default: <datadir>)",
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-blocknotify=<cmd>",
                 "Execute command when the best block changes (%s in cmd is "
                 "replaced by block hash)",
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-blockreconstructionextratxn=<n>",
                 strprintf("Extra transactions to keep in memory for compact "
                           "block reconstructions (default: %u)",
                           DEFAULT_BLOCK_RECONSTRUCTION_EXTRA_TXN),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-blocksonly",
                 strprintf("Whether to reject transactions from network peers. Transactions from the wallet or RPC are "
                           "not affected. (default: %d)",
                           DEFAULT_BLOCKSONLY),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-conf=<file>",
                 strprintf("Specify configuration file. Relative paths will be "
                           "prefixed by datadir location. (default: %s)",
                           RADIANT_CONF_FILENAME),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-datadir=<dir>", "Specify data directory", ArgsManager::ALLOW_ANY,
                 OptionsCategory::OPTIONS);
    gArgs.AddArg(
        "-dbbatchsize=<n>",
        strprintf("Maximum database write batch size in bytes (default: %u)",
                  nDefaultDbBatchSize),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::OPTIONS);
    gArgs.AddArg(
        "-dbcache=<n>",
        strprintf(
            "Set database cache size in megabytes (%d to %d, default: %d)",
            nMinDbCache, nMaxDbCache, nDefaultDbCache),
        ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-debuglogfile=<file>",
                 strprintf("Specify location of debug log file. Relative paths "
                           "will be prefixed by a net-specific datadir "
                           "location. (0 to disable, default: %s)",
                           DEFAULT_DEBUGLOGFILE),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-excessiveblocksize=<n>",
                 strprintf("Do not accept blocks larger than this limit, in "
                           "bytes (default: %u, testnet: %u, testnet4: %u, scalenet: %u, regtest: %u)",
                           defaultChainParams->GetConsensus().nDefaultExcessiveBlockSize,
                           testnetChainParams->GetConsensus().nDefaultExcessiveBlockSize,
                           testnet4ChainParams->GetConsensus().nDefaultExcessiveBlockSize,
                           scalenetChainParams->GetConsensus().nDefaultExcessiveBlockSize,
                           regtestChainParams->GetConsensus().nDefaultExcessiveBlockSize),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-feefilter",
                 strprintf("Tell other nodes to filter invs to us by our "
                           "mempool min fee (default: %d)",
                           DEFAULT_FEEFILTER),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-finalizationdelay=<n>",
                 strprintf("Set the minimum amount of time to wait between a "
                           "block header reception and the block finalization. "
                           "Unit is seconds (default: %d)",
                           DEFAULT_MIN_FINALIZATION_DELAY),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg(
        "-includeconf=<file>",
        "Specify additional configuration file, relative to the -datadir path "
        "(only useable from configuration file, not command line)",
        ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-maxreorgdepth=<n>",
                 strprintf("Configure at what depth blocks are considered "
                           "final (-1 to disable, default: %d)",
                           DEFAULT_MAX_REORG_DEPTH),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-finalizeheaders",
                 strprintf("Whether to reject new headers below maxreorgdepth "
                           "if a finalized block exists (default: %u)",
                           DEFAULT_FINALIZE_HEADERS),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-finalizeheaderspenalty=<n>",
                 strprintf("Penalize peers sending headers below with DoS score <n> "
                           "(default: %u)",
                           DEFAULT_FINALIZE_HEADERS_PENALTY),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-loadblock=<file>",
                 "Imports blocks from external blk000??.dat file on startup",
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-maxmempool=<n>", strprintf("Keep the transaction memory pool below <n> "
                 "megabytes (default: %u, testnet: %u, testnet4: %u, scalenet: %u)",
                 DEFAULT_MAX_MEMPOOL_SIZE_PER_MB * defaultChainParams->GetConsensus().nDefaultExcessiveBlockSize / ONE_MEGABYTE,
                 DEFAULT_MAX_MEMPOOL_SIZE_PER_MB * testnetChainParams->GetConsensus().nDefaultExcessiveBlockSize / ONE_MEGABYTE,
                 DEFAULT_MAX_MEMPOOL_SIZE_PER_MB * testnet4ChainParams->GetConsensus().nDefaultExcessiveBlockSize / ONE_MEGABYTE,
                 DEFAULT_MAX_MEMPOOL_SIZE_PER_MB * scalenetChainParams->GetConsensus().nDefaultExcessiveBlockSize / ONE_MEGABYTE),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-maxorphantx=<n>",
                 strprintf("Keep at most <n> unconnectable transactions in "
                           "memory (default: %u)",
                           DEFAULT_MAX_ORPHAN_TRANSACTIONS),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-mempoolexpiry=<n>",
                 strprintf("Do not keep transactions in the mempool longer "
                           "than <n> hours (default: %u)",
                           DEFAULT_MEMPOOL_EXPIRY),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-mempoolexpirytaskperiod=<n>",
                 strprintf("Execute the mempool expiry task this often in "
                           "hours (default: %u)",
                           DEFAULT_MEMPOOL_EXPIRY_TASK_PERIOD),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg(
        "-minimumchainwork=<hex>",
        strprintf(
            "Minimum work assumed to exist on a valid chain in hex "
            "(default: %s, testnet: %s)",
            defaultChainParams->GetConsensus().nMinimumChainWork.GetHex(),
            testnetChainParams->GetConsensus().nMinimumChainWork.GetHex()),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::OPTIONS);
    gArgs.AddArg(
        "-par=<n>",
        strprintf("Set the number of script verification threads (up to %d, 0 "
                  "= auto, <0 = leave that many cores free, default: %d)",
                  MAX_SCRIPTCHECK_THREADS,
                  DEFAULT_SCRIPTCHECK_THREADS),
        ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-parkdeepreorg",
                 strprintf("If connecting a new block would require rewinding "
                           "more than one block from the active chain (i.e., "
                           "a \"deep reorg\"), then mark the new block as "
                           "parked (default: %d)",
                           DEFAULT_PARK_DEEP_REORG),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-persistmempool",
                 strprintf("Whether to save the mempool on shutdown and load "
                           "on restart (default: %u)",
                           DEFAULT_PERSIST_MEMPOOL),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-pid=<file>",
                 strprintf("Specify pid file. Relative paths will be prefixed "
                           "by a net-specific datadir location. (default: %s)",
                           BITCOIN_PID_FILENAME),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg(
        "-prune=<n>",
        strprintf("Reduce storage requirements by enabling pruning (deleting) "
                  "of old blocks. This allows the pruneblockchain RPC to be "
                  "called to delete specific blocks, and enables automatic "
                  "pruning of old blocks if a target size in MiB is provided. "
                  "This mode is incompatible with -txindex and -rescan. "
                  "Warning: Reverting this setting requires re-downloading the "
                  "entire blockchain. (default: 0 = disable pruning blocks, 1 "
                  "= allow manual pruning via RPC, >=%u = automatically prune "
                  "block files to stay under the specified target size in MiB)",
                  MIN_DISK_SPACE_FOR_BLOCK_FILES / 1024 / 1024),
        ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-reindex-chainstate",
                 "Rebuild chain state from the currently indexed blocks. When "
                 "in pruning mode or if blocks on disk might be corrupted, use "
                 "full -reindex instead.",
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg(
        "-reindex",
        "Rebuild chain state and block index from the blk*.dat files on disk",
        ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
#ifndef WIN32
    gArgs.AddArg(
        "-sysperms",
        "Create new files with system default permissions, instead of umask "
        "077 (only effective with disabled wallet functionality)",
        ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
#else
    hidden_args.emplace_back("-sysperms");
#endif
    gArgs.AddArg("-txindex",
                 strprintf("Maintain a full transaction index, used by the "
                           "getrawtransaction rpc call (default: %d)",
                           DEFAULT_TXINDEX),
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
    gArgs.AddArg("-addnode=<ip>",
                 "Add a node to connect to and attempt to keep the connection "
                 "open (see the `addnode` RPC command help for more info)",
                 ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-asmap=<file>",
                 strprintf("Specify asn mapping used for bucketing of the peers (default: %s). Relative paths will be "
                           "prefixed by the net-specific datadir location.",
                           DEFAULT_ASMAP_FILENAME),
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-banscore=<n>",
                 strprintf("Threshold for disconnecting and discouraging misbehaving peers (default: %u)",
                           DEFAULT_BANSCORE_THRESHOLD),
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-bantime=<n>",
                 strprintf("Default bantime (in seconds) for manually configured bans (default: %u)",
                           DEFAULT_MANUAL_BANTIME),
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-bind=<addr>[:<port>][=onion]",
                 strprintf("Bind to given address and always listen on it (default: 0.0.0.0). Use [host]:port notation "
                           "for IPv6. Append =onion to tag any incoming connections to that address and port as "
                           "incoming Tor connections (default: 127.0.0.1:%u=onion, testnet: 127.0.0.1:%u=onion, "
                           "testnet4: 127.0.0.1:%u=onion, scalenet: 127.0.0.1:%u=onion, regtest: 127.0.0.1:%u=onion)",
                           defaultBaseParams->OnionServiceTargetPort(), testnetBaseParams->OnionServiceTargetPort(),
                           testnet4BaseParams->OnionServiceTargetPort(), scalenetBaseParams->OnionServiceTargetPort(),
                           regtestBaseParams->OnionServiceTargetPort()),
                 ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::CONNECTION);
    gArgs.AddArg(
        "-connect=<ip>",
        "Connect only to the specified node(s); -connect=0 disables automatic "
        "connections (the rules for this peer are the same as for -addnode)",
        ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-discover",
                 "Discover own IP addresses (default: 1 when listening and no "
                 "-externalip or -proxy)",
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-dns",
                 strprintf("Allow DNS lookups for -addnode, -seednode and "
                           "-connect (default: %d)",
                           DEFAULT_NAME_LOOKUP),
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-dnsseed",
                 "Query for peer addresses via DNS lookup, if low on addresses "
                 "(default: 1 unless -connect used)",
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-enablebip61",
                 strprintf("Send reject messages per BIP61 (default: %u)",
                           DEFAULT_ENABLE_BIP61),
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);

    gArgs.AddArg("-externalip=<ip>", "Specify your own public address", ArgsManager::ALLOW_ANY,
                 OptionsCategory::CONNECTION);
    gArgs.AddArg(
        "-forcednsseed",
        strprintf(
            "Always query for peer addresses via DNS lookup (default: %d)",
            DEFAULT_FORCEDNSSEED),
        ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg(
        "-listen",
        "Accept connections from outside (default: 1 if no -proxy or -connect)",
        ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-listenonion",
                 strprintf("Automatically create Tor onion service (default: %d)", DEFAULT_LISTEN_ONION),
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg(
        "-maxconnections=<n>",
        strprintf("Maintain at most <n> connections to peers (default: %u)",
                  DEFAULT_MAX_PEER_CONNECTIONS),
        ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-maxreceivebuffer=<n>",
                 strprintf("Maximum per-connection receive buffer, <n>*1000 "
                           "bytes (default: %u)",
                           DEFAULT_MAXRECEIVEBUFFER),
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg(
        "-maxsendbuffer=<n>",
        strprintf(
            "Maximum per-connection send buffer, <n>*1000 bytes (default: %u)",
            DEFAULT_MAXSENDBUFFER),
        ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg(
        "-maxtimeadjustment",
        strprintf("Maximum allowed median peer time offset adjustment. Local "
                  "perspective of time may be influenced by peers forward or "
                  "backward by this amount. (default: %u seconds)",
                  DEFAULT_MAX_TIME_ADJUSTMENT),
        ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-onion=<ip:port>",
                 strprintf("Use separate SOCKS5 proxy to reach peers via Tor onion services (default: %s)", "-proxy"),
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-onlynet=<net>",
                 "Only connect to nodes in network <net> (ipv4, ipv6 or onion)",
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-peerbloomfilters",
                 strprintf("Support filtering of blocks and transaction with "
                           "bloom filters (default: %d)",
                           DEFAULT_PEERBLOOMFILTERS),
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-port=<port>",
                 strprintf("Listen for connections on <port> (default: %u, "
                           "testnet: %u, testnet4: %u, scalenet: %u, regtest: %u)",
                           defaultChainParams->GetDefaultPort(),
                           testnetChainParams->GetDefaultPort(),
                           testnet4ChainParams->GetDefaultPort(),
                           scalenetChainParams->GetDefaultPort(),
                           regtestChainParams->GetDefaultPort()),
                 ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-proxy=<ip:port>", "Connect through SOCKS5 proxy", ArgsManager::ALLOW_ANY,
                 OptionsCategory::CONNECTION);
    gArgs.AddArg("-proxyrandomize",
                 strprintf("Randomize credentials for every proxy connection. "
                           "This enables Tor stream isolation (default: %d)",
                           DEFAULT_PROXYRANDOMIZE),
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-seednode=<ip>",
                 "Connect to a node to retrieve peer addresses, and disconnect",
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-timeout=<n>",
                 strprintf("Specify connection timeout in milliseconds "
                           "(minimum: 1, default: %d)",
                           DEFAULT_CONNECT_TIMEOUT),
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg(
        "-peertimeout=<n>",
        strprintf("Specify p2p connection timeout in seconds. This option "
                  "determines the amount of time a peer may be inactive before "
                  "the connection to it is dropped. (minimum: 1, default: %d)",
                  DEFAULT_PEER_CONNECT_TIMEOUT),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CONNECTION);
    gArgs.AddArg(
        "-torcontrol=<ip>:<port>",
        strprintf(
            "Tor control port to use if onion listening enabled (default: %s)",
            DEFAULT_TOR_CONTROL),
        ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-torpassword=<pass>",
                 "Tor control port password (default: empty)", ArgsManager::ALLOW_ANY,
                 OptionsCategory::CONNECTION);
#ifdef USE_UPNP
#if USE_UPNP
    gArgs.AddArg("-upnp",
                 "Use UPnP to map the listening port (default: 1 when "
                 "listening and no -proxy)",
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
#else
    gArgs.AddArg(
        "-upnp",
        strprintf("Use UPnP to map the listening port (default: %u)", 0), ArgsManager::ALLOW_ANY,
        OptionsCategory::CONNECTION);
#endif
#else
    hidden_args.emplace_back("-upnp");
#endif
    gArgs.AddArg("-whitebind=<addr>",
                 "Bind to given address and whitelist peers connecting to it. "
                 "Use [host]:port notation for IPv6",
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);
    gArgs.AddArg("-whitelist=<IP address or network>",
                 "Whitelist peers connecting from the given IP address (e.g. "
                 "1.2.3.4) or CIDR notated network (e.g. 1.2.3.0/24). Can be "
                 "specified multiple times. "
                 "Whitelisted peers cannot be DoS banned and their "
                 "transactions are always relayed, even if they are already in "
                 "the mempool, useful e.g. for a gateway",
                 ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);

    gArgs.AddArg(
        "-useextversion",
        strprintf("Enable extended versioning handshake (default: %d)",
            extversion::DEFAULT_ENABLED),
            ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);

    gArgs.AddArg(
        "-maxuploadtarget=<n>",
        strprintf("Tries to keep outbound traffic under the given target in "
                  "MiB per 24h (0 for no limit, default: %d)",
                  DEFAULT_MAX_UPLOAD_TARGET),
        ArgsManager::ALLOW_ANY, OptionsCategory::CONNECTION);

    g_wallet_init_interface.AddWalletOptions();

#if ENABLE_ZMQ
    gArgs.AddArg("-zmqpubhashblock=<address>",
                 "Enable publish hash block in <address>", ArgsManager::ALLOW_ANY,
                 OptionsCategory::ZMQ);
    gArgs.AddArg("-zmqpubhashtx=<address>",
                 "Enable publish hash transaction in <address>", ArgsManager::ALLOW_ANY,
                 OptionsCategory::ZMQ);
    gArgs.AddArg("-zmqpubrawblock=<address>",
                 "Enable publish raw block in <address>", ArgsManager::ALLOW_ANY,
                 OptionsCategory::ZMQ);
    gArgs.AddArg("-zmqpubrawtx=<address>",
                 "Enable publish raw transaction in <address>", ArgsManager::ALLOW_ANY,
                 OptionsCategory::ZMQ);
    gArgs.AddArg("-zmqpubhashds=<address>",
                 "Enable publish hash double spend transaction in <address>", ArgsManager::ALLOW_ANY,
                 OptionsCategory::ZMQ);
    gArgs.AddArg("-zmqpubrawds=<address>",
                 "Enable publish raw double spend transaction in <address>", ArgsManager::ALLOW_ANY,
                 OptionsCategory::ZMQ);
#else
    hidden_args.emplace_back("-zmqpubhashblock=<address>");
    hidden_args.emplace_back("-zmqpubhashtx=<address>");
    hidden_args.emplace_back("-zmqpubrawblock=<address>");
    hidden_args.emplace_back("-zmqpubrawtx=<address>");
    hidden_args.emplace_back("-zmqpubhashds=<address>");
    hidden_args.emplace_back("-zmqpubrawds=<address>");
#endif

    gArgs.AddArg(
        "-checkblocks=<n>",
        strprintf("How many blocks to check at startup (default: %u, 0 = all)",
                  DEFAULT_CHECKBLOCKS),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg(
        "-checklevel=<n>",
        strprintf("How thorough the block verification of "
                  "-checkblocks is: "
                  "level 0 reads the blocks from disk, "
                  "level 1 verifies block validity, "
                  "level 2 verifies undo data, "
                  "level 3 checks disconnection of tip blocks, "
                  "and level 4 tries to reconnect the blocks. "
                  "Each level includes the checks of the previous levels "
                  "(0-4, default: %u)",
                  DEFAULT_CHECKLEVEL),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg(
        "-checkblockindex",
        strprintf("Do a full consistency check for mapBlockIndex, "
                  "setBlockIndexCandidates, ::ChainActive() and "
                  "mapBlocksUnlinked occasionally. (default: %u, regtest: %u)",
                  defaultChainParams->DefaultConsistencyChecks(),
                  regtestChainParams->DefaultConsistencyChecks()),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg(
        "-checkmempool=<n>",
        strprintf(
            "Run checks every <n> transactions (default: %u, regtest: %u)",
            defaultChainParams->DefaultConsistencyChecks(),
            regtestChainParams->DefaultConsistencyChecks()),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-checkpoints",
                 strprintf("Only accept block chain matching built-in "
                           "checkpoints (default: %d)",
                           DEFAULT_CHECKPOINTS_ENABLED),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-deprecatedrpc=<method>",
                 "Allows deprecated RPC method(s) to be used", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY,
                 OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-dropmessagestest=<n>",
                 "Randomly drop 1 of every <n> network messages", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY,
                 OptionsCategory::DEBUG_TEST);
    gArgs.AddArg(
        "-stopafterblockimport",
        strprintf("Stop running after importing blocks from disk (default: %d)",
                  DEFAULT_STOPAFTERBLOCKIMPORT),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-stopatheight",
                 strprintf("Stop running after reaching the given height in "
                           "the main chain (default: %u)",
                           DEFAULT_STOPATHEIGHT),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-addrmantest", "Allows to test address relay on localhost",
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);

    gArgs.AddArg("-debug=<category>",
                 strprintf("Output debugging information (default: %u, supplying <category> is optional)", 0) +
                 ". If <category> is not supplied or if <category> = 1 or all, output all debugging information "
                 "(except for httptrace). <category> can be: " + ListLogCategories() + ".",
                 ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg(
        "-debugexclude=<category>",
        strprintf("Exclude debugging information for a category. Can be used "
                  "in conjunction with -debug=1 to output debug logs for all "
                  "categories except one or more specified categories."),
        ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-logips",
                 strprintf("Include IP addresses in debug output (default: %d)",
                           DEFAULT_LOGIPS),
                 ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-logtimestamps",
                 strprintf("Prepend debug output with timestamp (default: %d)",
                           DEFAULT_LOGTIMESTAMPS),
                 ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-logthreadnames", strprintf("Prepend debug output with name of the originating thread (only available on platforms supporting thread_local) (default: %u)", DEFAULT_LOGTHREADNAMES), ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);

    gArgs.AddArg(
        "-logtimemicros",
        strprintf("Add microsecond precision to debug timestamps (default: %d)",
                  DEFAULT_LOGTIMEMICROS),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg(
        "-mocktime=<n>",
        "Replace actual time with <n> seconds since epoch (default: 0)", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY,
        OptionsCategory::DEBUG_TEST);
    gArgs.AddArg(
        "-maxsigcachesize=<n>",
        strprintf("Limit size of signature cache to <n> MiB (0 to %d, default: %d)",
                  MAX_MAX_SIG_CACHE_SIZE, DEFAULT_MAX_SIG_CACHE_SIZE),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg(
        "-maxscriptcachesize=<n>",
        strprintf("Limit size of script cache to <n> MiB (0 to %d, default: %d)",
                  MAX_MAX_SCRIPT_CACHE_SIZE, DEFAULT_MAX_SCRIPT_CACHE_SIZE),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-maxtipage=<n>",
                 strprintf("Maximum tip age in seconds to consider node in "
                           "initial block download (default: %u)",
                           DEFAULT_MAX_TIP_AGE),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);

    gArgs.AddArg(
        "-printtoconsole",
        "Send trace/debug info to console instead of debug.log file (default: "
        "1 when no -daemon. To disable logging to file, set debuglogfile=0)",
        ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-printpriority",
                 strprintf("Log transaction priority and fee per kB when "
                           "mining blocks (default: %d)",
                           DEFAULT_PRINTPRIORITY),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg(
        "-shrinkdebugfile",
        "Shrink debug.log file on client startup (default: 1 when no -debug)",
        ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);

    gArgs.AddArg("-uacomment=<cmt>", "Append comment to the user agent string",
                 ArgsManager::ALLOW_ANY, OptionsCategory::DEBUG_TEST);
    gArgs.AddArg("-rejectsubversion=<substring>",
                 "Reject peers having a user agent string containing <substring> (case-sensitive)",
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::DEBUG_TEST);

    SetupChainParamsBaseOptions();

    gArgs.AddArg(
        "-acceptnonstdtxn",
        strprintf(
            "Relay and mine \"non-standard\" transactions (testnet/regtest only, default: %d)",
            defaultChainParams->RequireStandard()),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::NODE_RELAY);
    gArgs.AddArg(
        "-dustrelayfee=<amt>",
        strprintf("Fee rate (in %s/kB) used to defined dust, the value of an "
                  "output such that it will cost about 1/3 of its value in "
                  "fees at this fee rate to spend it. (default: %s)",
                  CURRENCY_UNIT, FormatMoney(DUST_RELAY_TX_FEE)),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::NODE_RELAY);

    gArgs.AddArg("-bytespersigcheck=<n>",
                 strprintf("Equivalent bytes per sigcheck in transactions for "
                           "relay and mining (default: %u)",
                           DEFAULT_BYTES_PER_SIGCHECK),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::NODE_RELAY);
    gArgs.AddArg("-bytespersigop=<n>",
                 strprintf("(Deprecated) Alias for -bytespersigcheck (default: %u)",
                           DEFAULT_BYTES_PER_SIGCHECK),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::NODE_RELAY);
    gArgs.AddArg("-datacarriersize=<n>",
                 strprintf("Maximum total size of OP_RETURN output scripts in a single transaction "
                           "we relay and mine (in bytes, 0 to reject all OP_RETURN transactions, default: %u)",
                           MAX_OP_RETURN_RELAY),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::NODE_RELAY);
    gArgs.AddArg("-permitbaremultisig",
                 strprintf("Relay non-P2SH multisig (default: %d)",
                           DEFAULT_PERMIT_BAREMULTISIG),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::NODE_RELAY);
    gArgs.AddArg(
        "-minrelaytxfee=<amt>",
        strprintf("Fees (in %s/kB) smaller than this are rejected for "
                  "relaying, mining and transaction creation (default: %s)",
                  CURRENCY_UNIT, FormatMoney(DEFAULT_MIN_RELAY_TX_FEE_PER_KB)),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::NODE_RELAY);
    gArgs.AddArg(
        "-txbroadcastinterval=<ms>",
        strprintf("Average time (in ms) between broadcasts of transaction inv "
                  "messages. Higher values reduce outbound bandwidth "
                  "dramatically by batching inv messages and reducing protocol "
                  "overhead. Lower values will help transactions propagate "
                  "faster. A value of 500 ms will begin to batch invs when tx "
                  "throughput approaches 2 tx/sec. (default: %d)",
                  DEFAULT_INV_BROADCAST_INTERVAL),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::NODE_RELAY);
    gArgs.AddArg(
        "-txbroadcastrate=<tx/sec/mb>",
        strprintf("Rate at which transaction invs can be broadcast, in terms "
                  "of the maximum block size. For example, a value of 7 with a "
                  "blocksize limit of 32 MB will result in a tx inv broadcast "
                  "rate of at most 224 tx/sec. (default: %d)",
                  DEFAULT_INV_BROADCAST_RATE),
        ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::NODE_RELAY);
    gArgs.AddArg(
        "-whitelistrelay",
        strprintf("Accept relayed transactions received from whitelisted "
                  "peers even when not relaying transactions (default: %d)",
                  DEFAULT_WHITELISTRELAY),
        ArgsManager::ALLOW_ANY, OptionsCategory::NODE_RELAY);
    gArgs.AddArg(
        "-whitelistforcerelay",
        strprintf("Force relay of transactions from whitelisted peers even if "
                  "they violate local relay policy (default: %d)",
                  DEFAULT_WHITELISTFORCERELAY),
        ArgsManager::ALLOW_ANY, OptionsCategory::NODE_RELAY);

    // Not sure this really belongs here, but it will do for now.
    // FIXME: This doesn't work anyways.
    gArgs.AddArg("-excessutxocharge=<amt>",
                 strprintf("Fees (in %s/kB) to charge per utxo created for "
                           "relaying, and mining (default: %s)",
                           CURRENCY_UNIT, FormatMoney(DEFAULT_UTXO_FEE)),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::NODE_RELAY);

    gArgs.AddArg("-blockmaxsize=<n>",
                 strprintf("Set maximum block size in bytes (default: %u, testnet: %u, testnet4: %u, scalenet: %u, "
                           "regtest: %u)",
                           defaultChainParams->GetConsensus().nDefaultGeneratedBlockSize,
                           testnetChainParams->GetConsensus().nDefaultGeneratedBlockSize,
                           testnet4ChainParams->GetConsensus().nDefaultGeneratedBlockSize,
                           scalenetChainParams->GetConsensus().nDefaultGeneratedBlockSize,
                           regtestChainParams->GetConsensus().nDefaultGeneratedBlockSize),
                 ArgsManager::ALLOW_ANY, OptionsCategory::BLOCK_CREATION);

    gArgs.AddArg("-maxgbttime=<n>",
                 strprintf("Maximum time (in ms, 0 for no limit) to spend "
                           "adding transactions to block templates in "
                           "'getblocktemplate' and 'generate' RPC calls "
                           "(default: %d)",
                           DEFAULT_MAX_GBT_TIME),
                 ArgsManager::ALLOW_ANY, OptionsCategory::BLOCK_CREATION);
    gArgs.AddArg("-maxinitialgbttime=<n>",
                 strprintf("Maximum time (in ms, 0 for no limit) to spend "
                           "adding transactions in the first getblocktemplate "
                           "(but not generate) call after receiving a new block"
                           " (default: %d). If -maxgbttime is stricter than "
                           "-maxinitialgbttime, then -maxinitialgbttime will"
                           "be ignored.",
                           DEFAULT_MAX_INITIAL_GBT_TIME),
                 ArgsManager::ALLOW_ANY, OptionsCategory::BLOCK_CREATION);

    gArgs.AddArg("-gbtcheckvalidity",
                 strprintf("Set whether to test generated block templates for validity in getblocktemplate and/or "
                           "getblocktemplatelight. Mining nodes may wish to skip validity checks as a performance "
                           "optimization, particularly when mining large blocks. Validity checking can also be set "
                           "on individual gbt calls by specifying the \"checkvalidity\": boolean key in the "
                           "template_request object given to gbt. (default: %d)", DEFAULT_GBT_CHECK_VALIDITY),
                 ArgsManager::ALLOW_ANY, OptionsCategory::BLOCK_CREATION);

    gArgs.AddArg("-blockmintxfee=<amt>",
                 strprintf("Set lowest fee rate (in %s/kB) for transactions to "
                           "be included in block creation. (default: %s)",
                           CURRENCY_UNIT,
                           FormatMoney(DEFAULT_BLOCK_MIN_TX_FEE_PER_KB)),
                 ArgsManager::ALLOW_ANY, OptionsCategory::BLOCK_CREATION);
    gArgs.AddArg("-blockversion=<n>",
                 "Override block version to test forking scenarios", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY,
                 OptionsCategory::BLOCK_CREATION);

    gArgs.AddArg("-banclientua=<ui>",
                 "Ban clients whose User Agent contains specified string (case insensitive). This option can be specified multiple times.", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY,
                 OptionsCategory::NODE_RELAY);

    gArgs.AddArg("-server", "Accept command line and JSON-RPC commands", ArgsManager::ALLOW_ANY,
                 OptionsCategory::RPC);
    gArgs.AddArg("-rest",
                 strprintf("Accept public REST requests (default: %d)",
                           DEFAULT_REST_ENABLE),
                 ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    gArgs.AddArg(
        "-rpcbind=<addr>[:port]",
        "Bind to given address to listen for JSON-RPC connections. This option "
        "is ignored unless -rpcallowip is also passed. Port is optional and "
        "overrides -rpcport. Use [host]:port notation for IPv6. This option "
        "can be specified multiple times (default: 127.0.0.1 and ::1 i.e., "
        "localhost, or if -rpcallowip has been specified, 0.0.0.0 and :: i.e., "
        "all addresses)",
        ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::RPC);
    gArgs.AddArg("-rpccookiefile=<loc>",
                 "Location of the auth cookie. Relative paths will be prefixed "
                 "by a net-specific datadir location. (default: data dir)",
                 ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    gArgs.AddArg("-rpcuser=<user>", "Username for JSON-RPC connections",
                 ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    gArgs.AddArg("-rpcpassword=<pw>", "Password for JSON-RPC connections",
                 ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    gArgs.AddArg(
        "-rpcauth=<userpw>",
        "Username and hashed password for JSON-RPC connections. The field "
        "<userpw> comes in the format: <USERNAME>:<SALT>$<HASH>. A canonical "
        "python script is included in share/rpcauth. The client then connects "
        "normally using the rpcuser=<USERNAME>/rpcpassword=<PASSWORD> pair of "
        "arguments. This option can be specified multiple times",
        ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    gArgs.AddArg("-rpcport=<port>",
                 strprintf("Listen for JSON-RPC connections on <port> "
                           "(default: %u, testnet: %u, testnet4: %u, scalenet: %u, regtest: %u)",
                           defaultBaseParams->RPCPort(),
                           testnetBaseParams->RPCPort(),
                           testnet4BaseParams->RPCPort(),
                           scalenetBaseParams->RPCPort(),
                           regtestBaseParams->RPCPort()),
                 ArgsManager::ALLOW_ANY | ArgsManager::NETWORK_ONLY, OptionsCategory::RPC);
    gArgs.AddArg("-rpcallowip=<ip>",
                 "Allow JSON-RPC connections from specified source. Valid for "
                 "<ip> are a single IP (e.g. 1.2.3.4), a network/netmask (e.g. "
                 "1.2.3.4/255.255.255.0) or a network/CIDR (e.g. 1.2.3.4/24). "
                 "This option can be specified multiple times",
                 ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    gArgs.AddArg(
        "-rpcthreads=<n>",
        strprintf(
            "Set the number of threads to service RPC calls (default: %d)",
            DEFAULT_HTTP_THREADS),
        ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    gArgs.AddArg(
        "-rpccorsdomain=value",
        "Domain from which to accept cross origin requests (browser enforced)",
        ArgsManager::ALLOW_ANY, OptionsCategory::RPC);

    gArgs.AddArg("-rpcworkqueue=<n>",
                 strprintf("Set the depth of the work queue to service RPC "
                           "calls (default: %d)",
                           DEFAULT_HTTP_WORKQUEUE),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::RPC);
    gArgs.AddArg("-rpcservertimeout=<n>",
                 strprintf("Timeout during HTTP requests (default: %d)",
                           DEFAULT_HTTP_SERVER_TIMEOUT),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::RPC);

#if HAVE_DECL_DAEMON
    gArgs.AddArg("-daemon",
                 "Run in the background as a daemon and accept commands",
                 ArgsManager::ALLOW_ANY, OptionsCategory::OPTIONS);
#else
    hidden_args.emplace_back("-daemon");
#endif

    // GBTLight args
    gArgs.AddArg("-gbtstoredir=<dir>",
                 strprintf("Specify a directory for storing getblocktemplatelight data (default: <datadir>/%s/)",
                           gbtl::DEFAULT_JOB_DATA_SUBDIR),
                 ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    gArgs.AddArg("-gbtcachesize=<n>",
                 strprintf("Specify how many recent getblocktemplatelight jobs to keep cached in memory (default: %d)",
                           gbtl::DEFAULT_JOB_CACHE_SIZE),
                 ArgsManager::ALLOW_ANY, OptionsCategory::RPC);
    gArgs.AddArg("-gbtstoretime=<secs>",
                 strprintf("Specify time in seconds to keep getblocktemplatelight data in the -gbtstoredir before it is "
                           "automatically deleted (0 to disable autodeletion, default: %d).",
                           gbtl::DEFAULT_JOB_DATA_EXPIRY_SECS),
                 ArgsManager::ALLOW_ANY, OptionsCategory::RPC);

    // Double Spend Proof
    gArgs.AddArg("-doublespendproof",
                 strprintf("Specify whether to enable or disable the double-spend proof subsystem. If enabled, the node"
                           " will send and receive double-spend proof messages (default: %d).",
                           DoubleSpendProof::IsEnabled()), ArgsManager::ALLOW_ANY, OptionsCategory::NODE_RELAY);

    // Add the hidden options
    gArgs.AddHiddenArgs(hidden_args);
}

std::string LicenseInfo() {
    constexpr auto URL_SOURCE_CODE = "<https://github.com/radiantblockchain/radiant-node>";
    constexpr auto URL_WEBSITE = "<https://radiantblockchain.org>";

    return CopyrightHolders(strprintf(_("Copyright (C) %i-%i"), 2009, COPYRIGHT_YEAR) + " ") +
           "\n\n" +
           strprintf(_("Please contribute if you find %s useful. Visit %s for further information about the software."),
                     PACKAGE_NAME, URL_WEBSITE) +
           "\n\n" +
           strprintf(_("The source code is available from %s."), URL_SOURCE_CODE) +
           "\n\n" +
           strprintf(_("Distributed under the Open Radiant (RAD) Version 1 software license, see the accompanying file %s"),
                     "COPYING") +
           "\n\n" +
           strprintf(_("This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit %s and "
                       "cryptographic software written by Eric Young and UPnP software written by Thomas Bernard."),
                     "<https://www.openssl.org>") +
#ifdef ENABLE_WALLET
           // Mention Berkeley DB version if built with wallet support.
           "\n\n" +
           strprintf(_("Using %s."), DbEnv::version(nullptr, nullptr, nullptr)) +
#endif
           "\n";
}

static void BlockNotifyCallback(bool initialSync,
                                const CBlockIndex *pBlockIndex) {
    if (initialSync || !pBlockIndex) {
        return;
    }

    std::string strCmd = gArgs.GetArg("-blocknotify", "");
    if (!strCmd.empty()) {
        ReplaceAll(strCmd, "%s", pBlockIndex->GetBlockHash().GetHex());
        std::thread t(runCommand, strCmd);
        // thread runs free
        t.detach();
    }
}

static bool fHaveGenesis = false;
static Mutex g_genesis_wait_mutex;
static std::condition_variable g_genesis_wait_cv;

static void BlockNotifyGenesisWait(bool, const CBlockIndex *pBlockIndex) {
    if (pBlockIndex != nullptr) {
        {
            LOCK(g_genesis_wait_mutex);
            fHaveGenesis = true;
        }
        g_genesis_wait_cv.notify_all();
    }
}

struct CImportingNow {
    CImportingNow() {
        assert(fImporting == false);
        fImporting = true;
    }

    ~CImportingNow() {
        assert(fImporting == true);
        fImporting = false;
    }
};

// If we're using -prune with -reindex, then delete block files that will be
// ignored by the reindex.  Since reindexing works by starting at block file 0
// and looping until a blockfile is missing, do the same here to delete any
// later block files after a gap. Also delete all rev files since they'll be
// rewritten by the reindex anyway. This ensures that vinfoBlockFile is in sync
// with what's actually on disk by the time we start downloading, so that
// pruning works correctly.
static void CleanupBlockRevFiles() {
    std::map<std::string, fs::path> mapBlockFiles;

    // Glob all blk?????.dat and rev?????.dat files from the blocks directory.
    // Remove the rev files immediately and insert the blk file paths into an
    // ordered map keyed by block file index.
    LogPrintf("Removing unusable blk?????.dat and rev?????.dat files for "
              "-reindex with -prune\n");
    const auto directoryIterator = fs::directory_iterator{GetBlocksDir()};
    for (const auto &file : directoryIterator) {
        const auto fileName = file.path().filename().string();
        if (fs::is_regular_file(file) && fileName.length() == 12 &&
            fileName.substr(8, 4) == ".dat") {
            if (fileName.substr(0, 3) == "blk") {
                mapBlockFiles[fileName.substr(3, 5)] = file.path();
            } else if (fileName.substr(0, 3) == "rev") {
                remove(file.path());
            }
        }
    }

    // Remove all block files that aren't part of a contiguous set starting at
    // zero by walking the ordered map (keys are block file indices) by keeping
    // a separate counter. Once we hit a gap (or if 0 doesn't exist) start
    // removing block files.
    int contiguousCounter = 0;
    for (const auto &item : mapBlockFiles) {
        if (atoi(item.first) == contiguousCounter) {
            contiguousCounter++;
            continue;
        }
        remove(item.second);
    }
}

static void ThreadImport(const Config &config,
                         std::vector<fs::path> vImportFiles) {
    util::ThreadRename("loadblk");
    ScheduleBatchPriority();

    {
        CImportingNow imp;

        // -reindex
        if (fReindex) {
            int nFile = 0;
            while (true) {
                FlatFilePos pos(nFile, 0);
                if (!fs::exists(GetBlockPosFilename(pos))) {
                    // No block files left to reindex
                    break;
                }
                FILE *file = OpenBlockFile(pos, true);
                if (!file) {
                    // This error is logged in OpenBlockFile
                    break;
                }
                LogPrintf("Reindexing block file blk%05u.dat...\n",
                          (unsigned int)nFile);
                LoadExternalBlockFile(config, file, &pos);
                nFile++;
            }
            pblocktree->WriteReindexing(false);
            fReindex = false;
            LogPrintf("Reindexing finished\n");
            // To avoid ending up in a situation without genesis block, re-try
            // initializing (no-op if reindexing worked):
            LoadGenesisBlock(config.GetChainParams());
        }

        // hardcoded $DATADIR/bootstrap.dat
        fs::path pathBootstrap = GetDataDir() / "bootstrap.dat";
        if (fs::exists(pathBootstrap)) {
            FILE *file = fsbridge::fopen(pathBootstrap, "rb");
            if (file) {
                fs::path pathBootstrapOld = GetDataDir() / "bootstrap.dat.old";
                LogPrintf("Importing bootstrap.dat...\n");
                LoadExternalBlockFile(config, file);
                RenameOver(pathBootstrap, pathBootstrapOld);
            } else {
                LogPrintf("Warning: Could not open bootstrap file %s\n",
                          pathBootstrap.string());
            }
        }

        // -loadblock=
        for (const fs::path &path : vImportFiles) {
            FILE *file = fsbridge::fopen(path, "rb");
            if (file) {
                LogPrintf("Importing blocks file %s...\n", path.string());
                LoadExternalBlockFile(config, file);
            } else {
                LogPrintf("Warning: Could not open blocks file %s\n",
                          path.string());
            }
        }

        // scan for better chains in the block chain database, that are not yet
        // connected in the active best chain
        CValidationState state;
        if (!ActivateBestChain(config, state)) {
            LogPrintf("Failed to connect best block (%s)\n",
                      FormatStateMessage(state));
            StartShutdown();
            return;
        }

        if (gArgs.GetBoolArg("-stopafterblockimport",
                             DEFAULT_STOPAFTERBLOCKIMPORT)) {
            LogPrintf("Stopping after block import\n");
            StartShutdown();
            return;
        }
    } // End scope of CImportingNow

    if (gArgs.GetArg("-persistmempool", DEFAULT_PERSIST_MEMPOOL)) {
        if (DoubleSpendProof::IsEnabled()) {
            LoadDSProofs(::g_mempool);
        }
        LoadMempool(config, ::g_mempool);
    }
    ::g_mempool.SetIsLoaded(!ShutdownRequested());
}

/** Sanity checks
 *  Ensure that Bitcoin is running in a usable environment with all
 *  necessary library support.
 */
static bool InitSanityCheck() {
    if (!ECC_InitSanityCheck()) {
        InitError(
            "Elliptic curve cryptography sanity check failure. Aborting.");
        return false;
    }

    if (!glibc_sanity_test() || !glibcxx_sanity_test()) {
        return false;
    }

    if (!Random_SanityCheck()) {
        InitError("OS cryptographic RNG sanity check failure. Aborting.");
        return false;
    }

    return true;
}

static bool AppInitServers(Config &config,
                           HTTPRPCRequestProcessor &httpRPCRequestProcessor) {
    RPCServerSignals::OnStarted(&OnRPCStarted);
    RPCServerSignals::OnStopped(&OnRPCStopped);
    if (!InitHTTPServer(config)) {
        return false;
    }

    try {
        gbtl::Initialize(scheduler);
    } catch (const std::exception &e) {
        return InitError(strprintf("Unable to initialize GBTLight subsystem: %s. Aborting.", e.what()));
    }

    StartRPC();

    if (!StartHTTPRPC(httpRPCRequestProcessor)) {
        return false;
    }
    if (gArgs.GetBoolArg("-rest", DEFAULT_REST_ENABLE)) {
        StartREST();
    }

    StartHTTPServer();
    return true;
}

// Parameter interaction based on rules
void InitParameterInteraction() {
    // when specifying an explicit binding address, you want to listen on it
    // even when -connect or -proxy is specified.
    if (gArgs.IsArgSet("-bind")) {
        if (gArgs.SoftSetBoolArg("-listen", true)) {
            LogPrintf(
                "%s: parameter interaction: -bind set -> setting -listen=1\n",
                __func__);
        }
    }
    if (gArgs.IsArgSet("-whitebind")) {
        if (gArgs.SoftSetBoolArg("-listen", true)) {
            LogPrintf("%s: parameter interaction: -whitebind set -> setting "
                      "-listen=1\n",
                      __func__);
        }
    }

    if (gArgs.IsArgSet("-connect")) {
        // when only connecting to trusted nodes, do not seed via DNS, or listen
        // by default.
        if (gArgs.SoftSetBoolArg("-dnsseed", false)) {
            LogPrintf("%s: parameter interaction: -connect set -> setting "
                      "-dnsseed=0\n",
                      __func__);
        }
        if (gArgs.SoftSetBoolArg("-listen", false)) {
            LogPrintf("%s: parameter interaction: -connect set -> setting "
                      "-listen=0\n",
                      __func__);
        }
    }

    if (gArgs.IsArgSet("-proxy")) {
        // to protect privacy, do not listen by default if a default proxy
        // server is specified.
        if (gArgs.SoftSetBoolArg("-listen", false)) {
            LogPrintf(
                "%s: parameter interaction: -proxy set -> setting -listen=0\n",
                __func__);
        }
        // to protect privacy, do not use UPNP when a proxy is set. The user may
        // still specify -listen=1 to listen locally, so don't rely on this
        // happening through -listen below.
        if (gArgs.SoftSetBoolArg("-upnp", false)) {
            LogPrintf(
                "%s: parameter interaction: -proxy set -> setting -upnp=0\n",
                __func__);
        }
        // to protect privacy, do not discover addresses by default
        if (gArgs.SoftSetBoolArg("-discover", false)) {
            LogPrintf("%s: parameter interaction: -proxy set -> setting "
                      "-discover=0\n",
                      __func__);
        }
    }

    if (!gArgs.GetBoolArg("-listen", DEFAULT_LISTEN)) {
        // do not map ports or try to retrieve public IP when not listening
        // (pointless)
        if (gArgs.SoftSetBoolArg("-upnp", false)) {
            LogPrintf(
                "%s: parameter interaction: -listen=0 -> setting -upnp=0\n",
                __func__);
        }
        if (gArgs.SoftSetBoolArg("-discover", false)) {
            LogPrintf(
                "%s: parameter interaction: -listen=0 -> setting -discover=0\n",
                __func__);
        }
        if (gArgs.SoftSetBoolArg("-listenonion", false)) {
            LogPrintf("%s: parameter interaction: -listen=0 -> setting "
                      "-listenonion=0\n",
                      __func__);
        }
    }

    if (gArgs.IsArgSet("-externalip")) {
        // if an explicit public IP is specified, do not try to find others
        if (gArgs.SoftSetBoolArg("-discover", false)) {
            LogPrintf("%s: parameter interaction: -externalip set -> setting "
                      "-discover=0\n",
                      __func__);
        }
    }

    // disable whitelistrelay in blocksonly mode
    if (gArgs.GetBoolArg("-blocksonly", DEFAULT_BLOCKSONLY)) {
        if (gArgs.SoftSetBoolArg("-whitelistrelay", false)) {
            LogPrintf("%s: parameter interaction: -blocksonly=1 -> setting "
                      "-whitelistrelay=0\n",
                      __func__);
        }
    }

    // Forcing relay from whitelisted hosts implies we will accept relays from
    // them in the first place.
    if (gArgs.GetBoolArg("-whitelistforcerelay", DEFAULT_WHITELISTFORCERELAY)) {
        if (gArgs.SoftSetBoolArg("-whitelistrelay", true)) {
            LogPrintf("%s: parameter interaction: -whitelistforcerelay=1 -> "
                      "setting -whitelistrelay=1\n",
                      __func__);
        }
    }
}

static std::string ResolveErrMsg(const char *const optname,
                                 const std::string &strBind) {
    return strprintf(_("Cannot resolve -%s address: '%s'"), optname, strBind);
}

/**
 * Initialize global loggers.
 *
 * Note that this is called very early in the process lifetime, so you should be
 * careful about what global state you rely on here.
 */
void InitLogging() {
    LogInstance().m_print_to_file = !gArgs.IsArgNegated("-debuglogfile");
    LogInstance().m_file_path = AbsPathForConfigVal(
        gArgs.GetArg("-debuglogfile", DEFAULT_DEBUGLOGFILE));

    // Add newlines to the logfile to distinguish this execution from the last
    // one; called before console logging is set up, so this is only sent to
    // debug.log.
    LogPrintf("\n\n\n\n\n");

    LogInstance().m_print_to_console = gArgs.GetBoolArg(
        "-printtoconsole", !gArgs.GetBoolArg("-daemon", false));
    LogInstance().m_log_timestamps =
        gArgs.GetBoolArg("-logtimestamps", DEFAULT_LOGTIMESTAMPS);
    LogInstance().m_log_time_micros =
        gArgs.GetBoolArg("-logtimemicros", DEFAULT_LOGTIMEMICROS);
    LogInstance().m_log_threadnames = gArgs.GetBoolArg("-logthreadnames", DEFAULT_LOGTHREADNAMES);

    fLogIPs = gArgs.GetBoolArg("-logips", DEFAULT_LOGIPS);

    std::string version_string = FormatFullVersion();
#ifdef DEBUG
    version_string += " (debug build)";
#else
    version_string += " (release build)";
#endif
    LogPrintf("%s version %s\n", CLIENT_NAME, version_string);
}

namespace { // Variables internal to initialization process only

int nMaxConnections;
int nUserMaxConnections;
int nFD;
ServiceFlags nLocalServices = ServiceFlags(NODE_NETWORK | NODE_NETWORK_LIMITED);
int64_t peer_connect_timeout;

} // namespace

[[noreturn]] static void new_handler_terminate() {
    // Rather than throwing std::bad-alloc if allocation fails, terminate
    // immediately to (try to) avoid chain corruption. Since LogPrintf may
    // itself allocate memory, set the handler directly to terminate first.
    std::set_new_handler(std::terminate);
    LogPrintf("Error: Out of memory. Terminating.\n");

    // The log was successful, terminate now.
    std::terminate();
};

bool AppInitBasicSetup() {
// Step 1: setup
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, nullptr,
                                             OPEN_EXISTING, 0, 0));
    // Disable confusing "helpful" text message on abort, Ctrl-C
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef WIN32
    // Enable Data Execution Prevention (DEP)
    SetProcessDEPPolicy(PROCESS_DEP_ENABLE);
#endif

    if (!SetupNetworking()) {
        return InitError("Initializing networking failed");
    }

#ifndef WIN32
    if (!gArgs.GetBoolArg("-sysperms", false)) {
        umask(077);
    }

    // Clean shutdown on SIGTERM
    registerSignalHandler(SIGTERM, HandleSIGTERM);
    registerSignalHandler(SIGINT, HandleSIGTERM);

    // Reopen debug.log on SIGHUP
    registerSignalHandler(SIGHUP, HandleSIGHUP);

    // Ignore SIGPIPE, otherwise it will bring the daemon down if the client
    // closes unexpectedly
    signal(SIGPIPE, SIG_IGN);
#else
    SetConsoleCtrlHandler(consoleCtrlHandler, true);
#endif

    std::set_new_handler(new_handler_terminate);

    return true;
}

bool AppInitParameterInteraction(Config &config) {
    const CChainParams &chainparams = config.GetChainParams();
    // Step 2: parameter interactions

    // also see: InitParameterInteraction()

    // Warn if network-specific options (-addnode, -connect, etc) are
    // specified in default section of config file, but not overridden
    // on the command line or in this network's section of the config file.
    std::string network = gArgs.GetChainName();
    for (const auto &arg : gArgs.GetUnsuitableSectionOnlyArgs()) {
        return InitError(strprintf(_("Config setting for %s only applied on %s "
                                     "network when in [%s] section."),
                                   arg, network, network));
    }

    // Warn if unrecognized section name are present in the config file.
    for (const auto &section : gArgs.GetUnrecognizedSections()) {
        InitWarning(strprintf("%s:%i " + _("Section [%s] is not recognized."),
                              section.m_file, section.m_line, section.m_name));
    }

    if (!fs::is_directory(GetBlocksDir())) {
        return InitError(
            strprintf(_("Specified blocks directory \"%s\" does not exist."),
                      gArgs.GetArg("-blocksdir", "").c_str()));
    }
    try{
        if (!fs::is_directory(GetIndexDir())) {
            return InitError(
                strprintf(_("Specified index directory \"%s\" does not exist."),
                            gArgs.GetArg("-indexdir", "").c_str()));
        }
    } catch (const fs::filesystem_error &e) {
        return InitError(
                strprintf("Error creating index directory: %s", e.what()));
    }

    // if using block pruning, then disallow txindex
    if (gArgs.GetArg("-prune", 0)) {
        if (gArgs.GetBoolArg("-txindex", DEFAULT_TXINDEX)) {
            return InitError(_("Prune mode is incompatible with -txindex."));
        }
    }

    // -bind and -whitebind can't be set when not listening
    size_t nUserBind =
        gArgs.GetArgs("-bind").size() + gArgs.GetArgs("-whitebind").size();
    if (nUserBind != 0 && !gArgs.GetBoolArg("-listen", DEFAULT_LISTEN)) {
        return InitError(
            "Cannot set -bind or -whitebind together with -listen=0");
    }

    // Make sure enough file descriptors are available
    int nBind = std::max(nUserBind, size_t(1));
    nUserMaxConnections =
        gArgs.GetArg("-maxconnections", DEFAULT_MAX_PEER_CONNECTIONS);
    nMaxConnections = std::max(nUserMaxConnections, 0);

    // Trim requested connection counts, to fit into system limitations
    // <int> in std::min<int>(...) to work around FreeBSD compilation issue
    // described in #2695
    nFD = RaiseFileDescriptorLimit(nMaxConnections + MIN_CORE_FILEDESCRIPTORS + MAX_ADDNODE_CONNECTIONS);
#ifdef USE_POLL
    int fd_max = nFD;
#else
    int fd_max = FD_SETSIZE;
#endif
    nMaxConnections = std::max(
        std::min<int>(nMaxConnections, fd_max - nBind - MIN_CORE_FILEDESCRIPTORS - MAX_ADDNODE_CONNECTIONS), 0);
    if (nFD < MIN_CORE_FILEDESCRIPTORS) {
        return InitError(_("Not enough file descriptors available."));
    }
    nMaxConnections =
        std::min(nFD - MIN_CORE_FILEDESCRIPTORS - MAX_ADDNODE_CONNECTIONS,
                 nMaxConnections);

    if (nMaxConnections < nUserMaxConnections) {
        InitWarning(strprintf(_("Reducing -maxconnections from %d to %d, "
                                "because of system limitations."),
                              nUserMaxConnections, nMaxConnections));
    }

    // Step 3: parameter-to-internal-flags
    if (gArgs.IsArgSet("-debug")) {
        // Special-case: if -debug=0/-nodebug is set, turn off debugging
        // messages
        const std::vector<std::string> &categories = gArgs.GetArgs("-debug");
        if (std::none_of(
                categories.begin(), categories.end(),
                [](const std::string &cat) { return cat == "0" || cat == "none"; })) {
            for (const auto &cat : categories) {
                if (!LogInstance().EnableCategory(cat)) {
                    InitWarning(
                        strprintf(_("Unsupported logging category %s=%s."),
                                  "-debug", cat));
                }
            }
        }
    }

    // Now remove the logging categories which were explicitly excluded
    for (const std::string &cat : gArgs.GetArgs("-debugexclude")) {
        if (!LogInstance().DisableCategory(cat)) {
            InitWarning(strprintf(_("Unsupported logging category %s=%s."),
                                  "-debugexclude", cat));
        }
    }

    // Checkmempool and checkblockindex default to true in regtest mode
    int ratio = std::min<int>(
        std::max<int>(
            gArgs.GetArg("-checkmempool",
                         chainparams.DefaultConsistencyChecks() ? 1 : 0),
            0),
        1000000);
    if (ratio != 0) {
        g_mempool.setSanityCheck(1.0 / ratio);
    }
    fCheckBlockIndex = gArgs.GetBoolArg("-checkblockindex",
                                        chainparams.DefaultConsistencyChecks());
    fCheckpointsEnabled =
        gArgs.GetBoolArg("-checkpoints", DEFAULT_CHECKPOINTS_ENABLED);
    if (fCheckpointsEnabled) {
        LogPrintf("Checkpoints will be verified.\n");
    } else {
        LogPrintf("Skipping checkpoint verification.\n");
    }

    if (gArgs.GetBoolArg("-finalizeheaders", DEFAULT_FINALIZE_HEADERS)
            && gArgs.GetArg("-maxreorgdepth", DEFAULT_MAX_REORG_DEPTH) > -1) {
        LogPrintf("New block headers below finalized block (maxreorgdepth=%d) will be rejected.\n",
                gArgs.GetArg("-maxreorgdepth", DEFAULT_MAX_REORG_DEPTH));
        const auto nFinalizeHeadersPenalty = gArgs.GetArg("-finalizeheaderspenalty", DEFAULT_FINALIZE_HEADERS_PENALTY);
        if (nFinalizeHeadersPenalty < 0 || nFinalizeHeadersPenalty > 100) {
                return InitError(strprintf(
                    "Invalid header finalization penalty (DoS score) (%s) - must be between 0 and 100",
                    nFinalizeHeadersPenalty));
        } else {
            LogPrintf("Nodes sending headers below finalized block will be penalized with DoS score %d.\n",
                    nFinalizeHeadersPenalty);
        }
    } else {
        LogPrintf("New block headers below finalized block may be accepted.\n");
    }

    hashAssumeValid = BlockHash::fromHex(
        gArgs.GetArg("-assumevalid",
                     chainparams.GetConsensus().defaultAssumeValid.GetHex()));
    if (!hashAssumeValid.IsNull()) {
        LogPrintf("Assuming ancestors of block %s have valid signatures.\n",
                  hashAssumeValid.GetHex());
    } else {
        LogPrintf("Validating signatures for all blocks.\n");
    }

    if (gArgs.IsArgSet("-minimumchainwork")) {
        const std::string minChainWorkStr =
            gArgs.GetArg("-minimumchainwork", "");
        if (!IsHexNumber(minChainWorkStr)) {
            return InitError(strprintf(
                "Invalid non-hex (%s) minimum chain work value specified",
                minChainWorkStr));
        }
        nMinimumChainWork = UintToArith256(uint256S(minChainWorkStr));
    } else {
        nMinimumChainWork =
            UintToArith256(chainparams.GetConsensus().nMinimumChainWork);
    }
    LogPrintf("Setting nMinimumChainWork=%s\n", nMinimumChainWork.GetHex());
    if (nMinimumChainWork <
        UintToArith256(chainparams.GetConsensus().nMinimumChainWork)) {
        LogPrintf("Warning: nMinimumChainWork set below default value of %s\n",
                  chainparams.GetConsensus().nMinimumChainWork.GetHex());
    }

    // Configure excessive block size.
    const uint64_t nProposedExcessiveBlockSize =
        gArgs.GetArg("-excessiveblocksize", chainparams.GetConsensus().nDefaultExcessiveBlockSize);
    if (!config.SetExcessiveBlockSize(nProposedExcessiveBlockSize)) {
        return InitError(
            _("Excessive block size must be > 1,000,000 bytes (1MB) and <= 2,000,000,000 bytes (2GB)."));
    }

    // Check blockmaxsize does not exceed maximum accepted block size.
    const uint64_t nProposedMaxGeneratedBlockSize =
        gArgs.GetArg("-blockmaxsize", chainparams.GetConsensus().nDefaultGeneratedBlockSize);
    if (!config.SetGeneratedBlockSize(nProposedMaxGeneratedBlockSize)) {
        auto msg = _("Max generated block size (blockmaxsize) cannot exceed "
                     "the excessive block size (excessiveblocksize)");
        return InitError(msg);
    }

    // mempool limits
    const int64_t nMempoolSizeMax =
        ONE_MEGABYTE * gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE_PER_MB * config.GetExcessiveBlockSize()
                                                   / ONE_MEGABYTE);
    if (nMempoolSizeMax < 0) {
        return InitError("-maxmempool must be at least 0 MB");
    } else {
        config.SetMaxMemPoolSize(uint64_t(nMempoolSizeMax));
    }

    // block pruning; get the amount of disk space (in MiB) to allot for block &
    // undo files
    int64_t nPruneArg = gArgs.GetArg("-prune", 0);
    if (nPruneArg < 0) {
        return InitError(
            _("Prune cannot be configured with a negative value."));
    }
    nPruneTarget = (uint64_t)nPruneArg * 1024 * 1024;
    if (nPruneArg == 1) {
        // manual pruning: -prune=1
        LogPrintf("Block pruning enabled.  Use RPC call "
                  "pruneblockchain(height) to manually prune block and undo "
                  "files.\n");
        nPruneTarget = std::numeric_limits<uint64_t>::max();
        fPruneMode = true;
    } else if (nPruneTarget) {
        if (nPruneTarget < MIN_DISK_SPACE_FOR_BLOCK_FILES) {
            return InitError(
                strprintf(_("Prune configured below the minimum of %d MiB.  "
                            "Please use a higher number."),
                          MIN_DISK_SPACE_FOR_BLOCK_FILES / 1024 / 1024));
        }
        LogPrintf("Prune configured to target %uMiB on disk for block and undo "
                  "files.\n",
                  nPruneTarget / 1024 / 1024);
        fPruneMode = true;
    }

    nConnectTimeout = gArgs.GetArg("-timeout", DEFAULT_CONNECT_TIMEOUT);
    if (nConnectTimeout <= 0) {
        nConnectTimeout = DEFAULT_CONNECT_TIMEOUT;
    }

    peer_connect_timeout =
        gArgs.GetArg("-peertimeout", DEFAULT_PEER_CONNECT_TIMEOUT);
    if (peer_connect_timeout <= 0) {
        return InitError(
            "peertimeout cannot be configured with a negative value.");
    }

    // Obtain the amount to charge excess UTXO
    if (gArgs.IsArgSet("-excessutxocharge")) {
        Amount n = Amount::zero();
        auto parsed = ParseMoney(gArgs.GetArg("-excessutxocharge", ""), n);
        if (!parsed || Amount::zero() > n) {
            return InitError(AmountErrMsg(
                "excessutxocharge", gArgs.GetArg("-excessutxocharge", "")));
        }
        config.SetExcessUTXOCharge(n);
    } else {
        config.SetExcessUTXOCharge(DEFAULT_UTXO_FEE);
    }

    if (gArgs.IsArgSet("-minrelaytxfee")) {
        Amount n = Amount::zero();
        auto parsed = ParseMoney(gArgs.GetArg("-minrelaytxfee", ""), n);
        if (!parsed || n == Amount::zero()) {
            return InitError(AmountErrMsg("minrelaytxfee",
                                          gArgs.GetArg("-minrelaytxfee", "")));
        }
        // High fee check is done afterward in WalletParameterInteraction()
        ::minRelayTxFee = CFeeRate(n);
    }

    const int64_t nTxBroadcastInterval = gArgs.GetArg("-txbroadcastinterval", DEFAULT_INV_BROADCAST_INTERVAL);
    if (nTxBroadcastInterval < 0) {
        return InitError(_("Transaction broadcast interval must not be configured with a negative value."));
    }
    if ( ! config.SetInvBroadcastInterval(nTxBroadcastInterval)) {
        return InitError("Transaction broadcast interval out of range.");
    }

    const int64_t nTxBroadcastRate = gArgs.GetArg("-txbroadcastrate", DEFAULT_INV_BROADCAST_RATE);
    if (nTxBroadcastRate < 0) {
        return InitError(_("Transaction broadcast rate must not be configured with a negative value."));
    }
    if ( ! config.SetInvBroadcastRate(nTxBroadcastRate)) {
        return InitError("Transaction broadcast rate out of range.");
    }

    // process and save -gbtcheckvalidity arg (if specified)
    config.SetGBTCheckValidity(gArgs.GetBoolArg("-gbtcheckvalidity", DEFAULT_GBT_CHECK_VALIDITY));

    // Sanity check argument for min fee for including tx in block
    // TODO: Harmonize which arguments need sanity checking and where that
    // happens.
    if (gArgs.IsArgSet("-blockmintxfee")) {
        Amount n = Amount::zero();
        if (!ParseMoney(gArgs.GetArg("-blockmintxfee", ""), n)) {
            return InitError(AmountErrMsg("blockmintxfee",
                                          gArgs.GetArg("-blockmintxfee", "")));
        }
    }

    // Feerate used to define dust.  Shouldn't be changed lightly as old
    // implementations may inadvertently create non-standard transactions.
    if (gArgs.IsArgSet("-dustrelayfee")) {
        Amount n = Amount::zero();
        auto parsed = ParseMoney(gArgs.GetArg("-dustrelayfee", ""), n);
        if (!parsed || Amount::zero() == n) {
            return InitError(AmountErrMsg("dustrelayfee",
                                          gArgs.GetArg("-dustrelayfee", "")));
        }
        dustRelayFee = CFeeRate(n);
    }

    fRequireStandard =
        !gArgs.GetBoolArg("-acceptnonstdtxn", !chainparams.RequireStandard());

    // -bytespersigcheck. Note that for legacy reasons we also support -bytespersigop, so
    // we must treat the two as aliases of each other.
    if (gArgs.IsArgSet("-bytespersigcheck") && gArgs.IsArgSet("-bytespersigop")) {
        return InitError("bytespersigcheck and bytespersigop may not both be specified at the same time");
    } else if (gArgs.IsArgSet("-bytespersigcheck")) {
        nBytesPerSigCheck = gArgs.GetArg("-bytespersigcheck", nBytesPerSigCheck);
    } else if (gArgs.IsArgSet("-bytespersigop")) {
        nBytesPerSigCheck = gArgs.GetArg("-bytespersigop", nBytesPerSigCheck);
    }

    if (!g_wallet_init_interface.ParameterInteraction()) {
        return false;
    }

    fIsBareMultisigStd = gArgs.GetBoolArg("-permitbaremultisig", DEFAULT_PERMIT_BAREMULTISIG);
    nMaxDatacarrierBytes = gArgs.GetArg("-datacarriersize", MAX_OP_RETURN_RELAY);

    // Option to startup with mocktime set (used for regression testing):
    SetMockTime(gArgs.GetArg("-mocktime", 0)); // SetMockTime(0) is a no-op

    if (gArgs.GetBoolArg("-peerbloomfilters", DEFAULT_PEERBLOOMFILTERS)) {
        nLocalServices = ServiceFlags(nLocalServices | NODE_BLOOM);
    }

    // Signal Radiant support.
    // TODO: remove some time after the hardfork when no longer needed
    // to differentiate the network nodes.
    nLocalServices = ServiceFlags(nLocalServices | NODE_BITCOIN_CASH);

    // option to use extversion
    // we do not use extversion by default
    if (gArgs.GetBoolArg("-useextversion", extversion::DEFAULT_ENABLED)) {
        nLocalServices = ServiceFlags(nLocalServices | NODE_EXTVERSION);
    }

    nMaxTipAge = gArgs.GetArg("-maxtipage", DEFAULT_MAX_TIP_AGE);

    // Option to enable/disable the double-spend proof subsystem (default: enabled)
    if (const bool def = DoubleSpendProof::IsEnabled(), en = gArgs.GetBoolArg("-doublespendproof", def); en != def) {
        DoubleSpendProof::SetEnabled(en);
    }

    return true;
}

static bool LockDataDirectory(bool probeOnly) {
    // Make sure only a single Bitcoin process is using the data directory.
    fs::path datadir = GetDataDir();
    if (!DirIsWritable(datadir)) {
        return InitError(strprintf(
            _("Cannot write to data directory '%s'; check permissions."),
            datadir.string()));
    }
    if (!LockDirectory(datadir, ".lock", probeOnly)) {
        return InitError(strprintf(_("Cannot obtain a lock on data directory "
                                     "%s. %s is probably already running."),
                                   datadir.string(), PACKAGE_NAME));
    }
    return true;
}

bool AppInitSanityChecks() {
    // Step 4: sanity checks

    // Initialize elliptic curve code
    std::string sha256_algo = SHA256AutoDetect();
    LogPrintf("Using the '%s' SHA256 implementation\n", sha256_algo);
    RandomInit();
    ECC_Start();
    globalVerifyHandle.reset(new ECCVerifyHandle());

    // Sanity check
    if (!InitSanityCheck()) {
        return InitError(strprintf(
            _("Initialization sanity check failed. %s is shutting down."),
            PACKAGE_NAME));
    }

    // Probe the data directory lock to give an early error message, if possible
    // We cannot hold the data directory lock here, as the forking for daemon()
    // hasn't yet happened, and a fork will cause weird behavior to it.
    return LockDataDirectory(true);
}

bool AppInitLockDataDirectory() {
    // After daemonization get the data directory lock again and hold on to it
    // until exit. This creates a slight window for a race condition to happen,
    // however this condition is harmless: it will at most make us exit without
    // printing a message to console.
    if (!LockDataDirectory(false)) {
        // Detailed error printed inside LockDataDirectory
        return false;
    }
    return true;
}

bool AppInitMain(Config &config, RPCServer &rpcServer,
                 HTTPRPCRequestProcessor &httpRPCRequestProcessor,
                 NodeContext &node) {
    // Step 4a: application initialization
    const CChainParams &chainparams = config.GetChainParams();

    if (!CreatePidFile()) {
        // Detailed error printed inside CreatePidFile().
        return false;
    }

    BCLog::Logger &logger = LogInstance();
    if (logger.m_print_to_file) {
        if (gArgs.GetBoolArg("-shrinkdebugfile",
                             logger.DefaultShrinkDebugFile())) {
            // Do this first since it both loads a bunch of debug.log into
            // memory, and because this needs to happen before any other
            // debug.log printing.
            logger.ShrinkDebugFile();
        }

        if (!logger.OpenDebugLog()) {
            return InitError(strprintf("Could not open debug log file %s",
                                       logger.m_file_path.string()));
        }
    }

    if (!logger.m_log_timestamps) {
        LogPrintf("Startup time: %s\n", FormatISO8601DateTime(GetTime()));
    }
    LogPrintf("Default data directory %s\n", GetDefaultDataDir().string());
    LogPrintf("Using data directory %s\n", GetDataDir().string());

    // Only log conf file usage message if conf file actually exists.
    fs::path config_file_path =
        GetConfigFile(gArgs.GetArg("-conf", RADIANT_CONF_FILENAME));
    if (fs::exists(config_file_path)) {
        LogPrintf("Config file: %s\n", config_file_path.string());
    } else if (gArgs.IsArgSet("-conf")) {
        // Warn if no conf file exists at path provided by user
        // Note: This branch left in place for safety. It cannot
        //       normally be taken because earlier in AppInit()
        //       we error-out in this case.
        InitWarning(
            strprintf(_("The specified config file %s does not exist"),
                      config_file_path.string()));
    } else {
        // Not categorizing as "Warning" because it's the default behavior
        LogPrintf("Config file: %s (not found, skipping)\n",
                  config_file_path.string());
    }

    LogPrintf("Using at most %i automatic connections (%i file descriptors "
              "available)\n",
              nMaxConnections, nFD);

    if (gArgs.IsArgSet("-banclientua"))
    {
        std::set<std::string> invalidUAClients;
        for (auto& invalidClient : gArgs.GetArgs("-banclientua"))
        {
            invalidUAClients.insert(std::move(invalidClient));
        }
        config.SetBanClientUA(std::move(invalidUAClients));
    }

    // Warn about relative -datadir path.
    if (gArgs.IsArgSet("-datadir") &&
        !fs::path(gArgs.GetArg("-datadir", "")).is_absolute()) {
        LogPrintf("Warning: relative datadir option '%s' specified, which will "
                  "be interpreted relative to the current working directory "
                  "'%s'. This is fragile, because if bitcoin is started in the "
                  "future from a different location, it will be unable to "
                  "locate the current data files. There could also be data "
                  "loss if bitcoin is started while in a temporary "
                  "directory.\n",
                  gArgs.GetArg("-datadir", ""), fs::current_path().string());
    }

    InitSignatureCache();
    InitScriptExecutionCache();

    int script_threads = gArgs.GetArg("-par", DEFAULT_SCRIPTCHECK_THREADS);
    if (script_threads <= 0) {
        // -par=0 means autodetect (number of cores - 1 script threads)
        // -par=-n means "leave n cores free" (number of cores - n - 1 script threads)
        script_threads += GetNumCores();
    }

    // Subtract 1 because the main thread counts towards the par threads
    script_threads = std::max(script_threads - 1, 0);

    // Number of script-checking threads <= MAX_SCRIPTCHECK_THREADS
    script_threads = std::min(script_threads, MAX_SCRIPTCHECK_THREADS);

    LogPrintf("Script verification uses %d additional threads\n", script_threads);
    if (script_threads >= 1) {
        StartScriptCheckWorkerThreads(script_threads);
    }

    // Start the lightweight task scheduler thread
    CScheduler::Function serviceLoop =
        std::bind(&CScheduler::serviceQueue, &scheduler);
    threadGroup.create_thread(std::bind(&TraceThread<CScheduler::Function>,
                                        "scheduler", serviceLoop));

    GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);
    GetMainSignals().RegisterWithMempoolSignals(g_mempool);

    // Create client interfaces for wallets that are supposed to be loaded
    // according to -wallet and -disablewallet options. This only constructs
    // the interfaces, it doesn't load wallet data. Wallets actually get loaded
    // when load() and start() interface methods are called below.
    g_wallet_init_interface.Construct(node);

    // Register the special submitblock state catcher validation interface before
    // we start RPC
    rpc::RegisterSubmitBlockCatcher();

    /**
     * Register RPC commands regardless of -server setting so they will be
     * available in the GUI RPC console even if external calls are disabled.
     */
    RegisterAllRPCCommands(config, rpcServer, tableRPC);
    for (const auto &client : node.chain_clients) {
        client->registerRpcs();
    }
    g_rpc_node = &node;
#if ENABLE_ZMQ
    RegisterZMQRPCCommands(tableRPC);
#endif

    software_outdated::nTime = 0;

    /**
     * Start the RPC server.  It will be started in "warmup" mode and not
     * process calls yet (but it will verify that the server is there and will
     * be ready later).  Warmup mode will be completed when initialisation is
     * finished.
     */
    if (gArgs.GetBoolArg("-server", false)) {
        uiInterface.InitMessage_connect(SetRPCWarmupStatus);
        if (!AppInitServers(config, httpRPCRequestProcessor)) {
            return InitError(
                _("Unable to start HTTP server. See debug log for details."));
        }
    }

    /// If the double-spend proof subsystem is enabled, enable the periodic dsproof orphan cleaner task.
    if (DoubleSpendProof::IsEnabled()) {
        auto *dspStorage = g_mempool.doubleSpendProofStorage();
        assert(dspStorage != nullptr);
        scheduler.scheduleEvery(std::bind(&DoubleSpendProofStorage::periodicCleanup, dspStorage), 60 * 1000);
    }

    /// Install the mempool expiry task which runs every -mempoolexpirytaskperiod (once a day by default).
    if (int64_t const expiryTimeHours = gArgs.GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY);
        expiryTimeHours > 0 && expiryTimeHours < 1'000'000) {
        int64_t const expiryTimeSecs = expiryTimeHours * 60 * 60;
        // Clamp the task period such that it can never be larger than half the -mempoolexpiry time
        // (or smaller than once per second).
        int64_t const taskIntervalMsec =
            std::clamp(gArgs.GetArg("-mempoolexpirytaskperiod", DEFAULT_MEMPOOL_EXPIRY_TASK_PERIOD) * 60 * 60 * 1000,
                       int64_t{1000},
                       (expiryTimeSecs * 1000) / 2);
        scheduler.scheduleEvery([expiryTimeSecs]{
            int64_t const timePoint = GetTime() - expiryTimeSecs;
            g_mempool.Expire(timePoint, false);
            return true;
        }, taskIntervalMsec);
        LogPrint(BCLog::MEMPOOL, "Mempool expiry task installed with a task period of %i msec\n", taskIntervalMsec);
        if (taskIntervalMsec < 10'000) {
            // warn if the mempool expiry task was set to run more often than every 10 seconds, since this may mean
            // the user misunderstood the scaling or meaning of the arg (in most cases nobody wants the task to run this
            // frequently).
            LogPrintf("WARNING: The mempool expiry task was configured to run every ~%i milliseconds, "
                      "which is extremely frequently. Please verify that this is what was intended.\n",
                      taskIntervalMsec);
        }
    } else {
        // Prevent overflow and UB: constrain -mempoolexpiry to a sane, positive value within the next ~114 years.
        return InitError("Invalid -mempoolexpiry argument. Please specify a value >0 and <1,000,000.");
    }


    // Step 5: verify wallet database integrity
    for (const auto &client : node.chain_clients) {
        if (!client->verify(chainparams)) {
            return false;
        }
    }

    // Step 6: network initialization

    // Note that we absolutely cannot open any actual connections
    // until the very end ("start node") as the UTXO/block state
    // is not yet setup and may end up being set up twice if we
    // need to reindex later.

    assert(!g_banman);
    g_banman = std::make_unique<BanMan>(
        GetDataDir() / "banlist.dat", config.GetChainParams(), &uiInterface,
        gArgs.GetArg("-bantime", DEFAULT_MANUAL_BANTIME));
    assert(!g_connman);
    g_connman = std::make_unique<CConnman>(
        config, GetRand(std::numeric_limits<uint64_t>::max()),
        GetRand(std::numeric_limits<uint64_t>::max()));

    peerLogic.reset(new PeerLogicValidation(
        g_connman.get(), g_banman.get(), scheduler,
        gArgs.GetBoolArg("-enablebip61", DEFAULT_ENABLE_BIP61)));
    RegisterValidationInterface(peerLogic.get());

    // sanitize comments per BIP-0014, format user agent and check total size
    std::vector<std::string> uacomments;
    for (const std::string &cmt : gArgs.GetArgs("-uacomment")) {
        if (cmt != SanitizeString(cmt, SAFE_CHARS_UA_COMMENT)) {
            return InitError(strprintf(
                _("User Agent comment (%s) contains unsafe characters."), cmt));
        }
        uacomments.push_back(cmt);
    }
    const std::string strSubVersion =
        FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, uacomments);
    if (strSubVersion.size() > MAX_SUBVERSION_LENGTH) {
        return InitError(strprintf(
            _("Total length of network version string (%i) exceeds maximum "
              "length (%i). Reduce the number or size of uacomments."),
            strSubVersion.size(), MAX_SUBVERSION_LENGTH));
    }

    if (gArgs.IsArgSet("-rejectsubversion")) {
        std::set<std::string> rejectSubVers;
        for (const auto &reject : gArgs.GetArgs("-rejectsubversion")) {
            if (reject.empty()) continue;
            rejectSubVers.insert(reject);
        }
        config.SetRejectSubVersions(rejectSubVers);
    }

    if (gArgs.IsArgSet("-onlynet")) {
        std::set<enum Network> nets;
        for (const std::string &snet : gArgs.GetArgs("-onlynet")) {
            enum Network net = ParseNetwork(snet);
            if (net == NET_UNROUTABLE) {
                return InitError(strprintf(
                    _("Unknown network specified in -onlynet: '%s'"), snet));
            }
            nets.insert(net);
        }
        for (int n = 0; n < NET_MAX; n++) {
            enum Network net = (enum Network)n;
            if (!nets.count(net)) {
                SetReachable(net, false);
            }
        }
    }

    // Check for host lookup allowed before parsing any network related
    // parameters
    fNameLookup = gArgs.GetBoolArg("-dns", DEFAULT_NAME_LOOKUP);

    bool proxyRandomize =
        gArgs.GetBoolArg("-proxyrandomize", DEFAULT_PROXYRANDOMIZE);
    // -proxy sets a proxy for all outgoing network traffic
    // -noproxy (or -proxy=0) as well as the empty string can be used to not set
    // a proxy, this is the default
    std::string proxyArg = gArgs.GetArg("-proxy", "");
    SetReachable(NET_ONION, false);
    if (proxyArg != "" && proxyArg != "0") {
        CService proxyAddr;
        if (!Lookup(proxyArg, proxyAddr, 9050, fNameLookup)) {
            return InitError(strprintf(
                _("Invalid -proxy address or hostname: '%s'"), proxyArg));
        }

        proxyType addrProxy = proxyType(proxyAddr, proxyRandomize);
        if (!addrProxy.IsValid()) {
            return InitError(strprintf(
                _("Invalid -proxy address or hostname: '%s'"), proxyArg));
        }

        SetProxy(NET_IPV4, addrProxy);
        SetProxy(NET_IPV6, addrProxy);
        SetProxy(NET_ONION, addrProxy);
        SetNameProxy(addrProxy);
        // by default, -proxy sets onion as reachable, unless -noonion later
        SetReachable(NET_ONION, true);
    }

    // -onion can be used to set only a proxy for .onion, or override normal
    // proxy for .onion addresses.
    // -noonion (or -onion=0) disables connecting to .onion entirely. An empty
    // string is used to not override the onion proxy (in which case it defaults
    // to -proxy set above, or none)
    std::string onionArg = gArgs.GetArg("-onion", "");
    if (onionArg != "") {
        if (onionArg == "0") {
            // Handle -noonion/-onion=0
            SetReachable(NET_ONION, false);
        } else {
            CService onionProxy;
            if (!Lookup(onionArg, onionProxy, 9050, fNameLookup)) {
                return InitError(strprintf(
                    _("Invalid -onion address or hostname: '%s'"), onionArg));
            }
            proxyType addrOnion = proxyType(onionProxy, proxyRandomize);
            if (!addrOnion.IsValid()) {
                return InitError(strprintf(
                    _("Invalid -onion address or hostname: '%s'"), onionArg));
            }
            SetProxy(NET_ONION, addrOnion);
            SetReachable(NET_ONION, true);
        }
    }

    // see Step 2: parameter interactions for more information about these
    fListen = gArgs.GetBoolArg("-listen", DEFAULT_LISTEN);
    fDiscover = gArgs.GetBoolArg("-discover", true);
    g_relay_txes = !gArgs.GetBoolArg("-blocksonly", DEFAULT_BLOCKSONLY);

    for (const std::string &strAddr : gArgs.GetArgs("-externalip")) {
        CService addrLocal;
        if (Lookup(strAddr, addrLocal, GetListenPort(), fNameLookup) && addrLocal.IsValid()) {
            AddLocal(addrLocal, LOCAL_MANUAL);
        } else {
            return InitError(ResolveErrMsg("externalip", strAddr));
        }
    }

    // Read asmap file if configured
    if (gArgs.IsArgSet("-asmap")) {
        fs::path asmap_path = fs::path(gArgs.GetArg("-asmap", ""));
        if (asmap_path.empty()) {
            asmap_path = DEFAULT_ASMAP_FILENAME;
        }
        if (!asmap_path.is_absolute()) {
            asmap_path = GetDataDir() / asmap_path;
        }
        if (!fs::exists(asmap_path)) {
            InitError(strprintf(_("Could not find asmap file %s"), asmap_path));
            return false;
        }
        std::vector<bool> asmap = CAddrMan::DecodeAsmap(asmap_path);
        if (asmap.size() == 0) {
            InitError(strprintf(_("Could not parse asmap file %s"), asmap_path));
            return false;
        }
        const uint256 asmap_version = SerializeHash(asmap);
        g_connman->SetAsmap(std::move(asmap));
        LogPrintf("Using asmap version %s for IP bucketing\n", asmap_version.ToString());
    } else {
        LogPrintf("Using /16 prefix for IP bucketing\n");
    }

#if ENABLE_ZMQ
    g_zmq_notification_interface = CZMQNotificationInterface::Create();

    if (g_zmq_notification_interface) {
        RegisterValidationInterface(g_zmq_notification_interface);
    }
#endif
    // unlimited unless -maxuploadtarget is set
    uint64_t nMaxOutboundLimit = 0;
    uint64_t nMaxOutboundTimeframe = MAX_UPLOAD_TIMEFRAME;

    if (gArgs.IsArgSet("-maxuploadtarget")) {
        nMaxOutboundLimit =
            gArgs.GetArg("-maxuploadtarget", DEFAULT_MAX_UPLOAD_TARGET) * 1024 *
            1024;
    }

    // Step 7: load block chain

    fReindex = gArgs.GetBoolArg("-reindex", false);
    bool fReindexChainState = gArgs.GetBoolArg("-reindex-chainstate", false);

    // cache size calculations
    int64_t nTotalCache = (gArgs.GetArg("-dbcache", nDefaultDbCache) << 20);
    // total cache cannot be less than nMinDbCache
    nTotalCache = std::max(nTotalCache, nMinDbCache << 20);
    // total cache cannot be greater than nMaxDbcache
    nTotalCache = std::min(nTotalCache, nMaxDbCache << 20);
    int64_t nBlockTreeDBCache =
        std::min(nTotalCache / 8, nMaxBlockDBCache << 20);
    nTotalCache -= nBlockTreeDBCache;
    int64_t nTxIndexCache =
        std::min(nTotalCache / 8, gArgs.GetBoolArg("-txindex", DEFAULT_TXINDEX)
                                      ? nMaxTxIndexCache << 20
                                      : 0);
    nTotalCache -= nTxIndexCache;
    // use 25%-50% of the remainder for disk cache
    int64_t nCoinDBCache =
        std::min(nTotalCache / 2, (nTotalCache / 4) + (1 << 23));
    // cap total coins db cache
    nCoinDBCache = std::min(nCoinDBCache, nMaxCoinsDBCache << 20);
    nTotalCache -= nCoinDBCache;
    // the rest goes to in-memory cache
    nCoinCacheUsage = nTotalCache;
    int64_t nMempoolSizeMax = config.GetMaxMemPoolSize();
    LogPrintf("Cache configuration:\n");
    LogPrintf("* Using %.1fMiB for block index database\n",
              nBlockTreeDBCache * (1.0 / 1024 / 1024));
    if (gArgs.GetBoolArg("-txindex", DEFAULT_TXINDEX)) {
        LogPrintf("* Using %.1fMiB for transaction index database\n",
                  nTxIndexCache * (1.0 / 1024 / 1024));
    }
    LogPrintf("* Using %.1fMiB for chain state database\n",
              nCoinDBCache * (1.0 / 1024 / 1024));
    LogPrintf("* Using %.1fMiB for in-memory UTXO set (plus up to %.1fMiB of "
              "unused mempool space)\n",
              nCoinCacheUsage * (1.0 / 1024 / 1024),
              nMempoolSizeMax * (1.0 / 1024 / 1024));

    int64_t nStart = 0;
    bool fLoaded = false;
    while (!fLoaded && !ShutdownRequested()) {
        const bool fReset = fReindex;
        std::string strLoadError;

        uiInterface.InitMessage(_("Loading block index..."));
        nStart = GetTimeMillis();
        do {
            try {
                LOCK(cs_main);
                UnloadBlockIndex();
                pcoinsTip.reset();
                pcoinsdbview.reset();
                pcoinscatcher.reset();
                // new CBlockTreeDB tries to delete the existing file, which
                // fails if it's still open from the previous loop. Close it
                // first:
                pblocktree.reset();
                pblocktree.reset(
                    new CBlockTreeDB(nBlockTreeDBCache, false, fReset));

                if (fReset) {
                    pblocktree->WriteReindexing(true);
                    // If we're reindexing in prune mode, wipe away unusable
                    // block files and all undo data files
                    if (fPruneMode) {
                        CleanupBlockRevFiles();
                    }
                }

                if (ShutdownRequested()) {
                    break;
                }

                // LoadBlockIndex will load fHavePruned if we've ever removed a
                // block file from disk.
                // Note that it also sets fReindex based on the disk flag!
                // From here on out fReindex and fReset mean something
                // different!
                if (!LoadBlockIndex(config)) {
                    strLoadError = _("Error loading block database");
                    break;
                }

                // If the loaded chain has a wrong genesis, bail out immediately
                // (we're likely using a testnet datadir, or the other way
                // around).
                if (!mapBlockIndex.empty() &&
                    !LookupBlockIndex(
                        chainparams.GetConsensus().hashGenesisBlock)) {
                    return InitError(_("Incorrect or no genesis block found. "
                                       "Wrong datadir for network?"));
                }

                // Check for changed -prune state.  What we are concerned about
                // is a user who has pruned blocks in the past, but is now
                // trying to run unpruned.
                if (fHavePruned && !fPruneMode) {
                    strLoadError =
                        _("You need to rebuild the database using -reindex to "
                          "go back to unpruned mode.  This will redownload the "
                          "entire blockchain");
                    break;
                }

                // At this point blocktree args are consistent with what's on
                // disk. If we're not mid-reindex (based on disk + args), add a
                // genesis block on disk (otherwise we use the one already on
                // disk).
                // This is called again in ThreadImport after the reindex
                // completes.
                if (!fReindex && !LoadGenesisBlock(chainparams)) {
                    strLoadError = _("Error initializing block database");
                    break;
                }

                // At this point we're either in reindex or we've loaded a
                // useful block tree into mapBlockIndex!

                pcoinsdbview.reset(new CCoinsViewDB(
                    nCoinDBCache, false, fReset || fReindexChainState));
                pcoinscatcher.reset(
                    new CCoinsViewErrorCatcher(pcoinsdbview.get()));

                // If necessary, upgrade from older database format.
                // This is a no-op if we cleared the coinsviewdb with -reindex
                // or -reindex-chainstate
                if (!pcoinsdbview->Upgrade()) {
                    strLoadError = _("Error upgrading chainstate database");
                    break;
                }

                // ReplayBlocks is a no-op if we cleared the coinsviewdb with
                // -reindex or -reindex-chainstate
                if (!ReplayBlocks(chainparams.GetConsensus(),
                                  pcoinsdbview.get())) {
                    strLoadError =
                        _("Unable to replay blocks. You will need to rebuild "
                          "the database using -reindex-chainstate.");
                    break;
                }

                // The on-disk coinsdb is now in a good state, create the cache
                pcoinsTip.reset(new CCoinsViewCache(pcoinscatcher.get()));

                bool is_coinsview_empty = fReset || fReindexChainState ||
                                          pcoinsTip->GetBestBlock().IsNull();
                if (!is_coinsview_empty) {
                    // LoadChainTip sets ::ChainActive() based on pcoinsTip's
                    // best block
                    if (!LoadChainTip(config)) {
                        strLoadError = _("Error initializing block database");
                        break;
                    }
                    assert(::ChainActive().Tip() != nullptr);

                    uiInterface.InitMessage(_("Verifying blocks..."));
                    if (fHavePruned &&
                        gArgs.GetArg("-checkblocks", DEFAULT_CHECKBLOCKS) >
                            MIN_BLOCKS_TO_KEEP) {
                        LogPrintf(
                            "Prune: pruned datadir may not have more than %d "
                            "blocks; only checking available blocks\n",
                            MIN_BLOCKS_TO_KEEP);
                    }

                    CBlockIndex *tip = ::ChainActive().Tip();
                    RPCNotifyBlockChange(true, tip);
                    if (tip && tip->nTime >
                                   GetAdjustedTime() + MAX_FUTURE_BLOCK_TIME) {
                        strLoadError = _(
                            "The block database contains a block which appears "
                            "to be from the future. This may be due to your "
                            "computer's date and time being set incorrectly. "
                            "Only rebuild the block database if you are sure "
                            "that your computer's date and time are correct");
                        break;
                    }

                    if (!CVerifyDB().VerifyDB(
                            config, pcoinsdbview.get(),
                            gArgs.GetArg("-checklevel", DEFAULT_CHECKLEVEL),
                            gArgs.GetArg("-checkblocks",
                                         DEFAULT_CHECKBLOCKS))) {
                        strLoadError = _("Corrupted block database detected");
                        break;
                    }
                }
            } catch (const std::exception &e) {
                LogPrintf("%s\n", e.what());
                strLoadError = _("Error opening block database");
                break;
            }

            fLoaded = true;
        } while (false);

        if (!fLoaded && !ShutdownRequested()) {
            // first suggest a reindex
            if (!fReset) {
                bool fRet = uiInterface.ThreadSafeQuestion(
                    strLoadError + ".\n\n" +
                        _("Do you want to rebuild the block database now?"),
                    strLoadError + ".\nPlease restart with -reindex or "
                                   "-reindex-chainstate to recover.",
                    "",
                    CClientUIInterface::MSG_ERROR |
                        CClientUIInterface::BTN_ABORT);
                if (fRet) {
                    fReindex = true;
                    AbortShutdown();
                } else {
                    LogPrintf("Aborted block database rebuild. Exiting.\n");
                    return false;
                }
            } else {
                return InitError(strLoadError);
            }
        }
    }

    // As LoadBlockIndex can take several minutes, it's possible the user
    // requested to kill the GUI during the last operation. If so, exit.
    // As the program has not fully started yet, Shutdown() is possibly
    // overkill.
    if (ShutdownRequested()) {
        LogPrintf("Shutdown requested. Exiting.\n");
        return false;
    }
    if (fLoaded) {
        LogPrintf(" block index %15dms\n", GetTimeMillis() - nStart);
    }

    // Encoded addresses using cashaddr instead of base58.
    // We do this by default to avoid confusion with BTC addresses.
    config.SetCashAddrEncoding(gArgs.GetBoolArg("-usecashaddr", DEFAULT_USE_CASHADDR));

    // Step 8: load indexers
    if (gArgs.GetBoolArg("-txindex", DEFAULT_TXINDEX)) {
        g_txindex = std::make_unique<TxIndex>(nTxIndexCache, false, fReindex);
        g_txindex->Start();
    }

    // Step 9: load wallet
    for (const auto &client : node.chain_clients) {
        if (!client->load(chainparams)) {
            return false;
        }
    }

    // Step 10: data directory maintenance

    // if pruning, unset the service bit and perform the initial blockstore
    // prune after any wallet rescanning has taken place.
    if (fPruneMode) {
        LogPrintf("Unsetting NODE_NETWORK on prune mode\n");
        nLocalServices = ServiceFlags(nLocalServices & ~NODE_NETWORK);
        if (!fReindex) {
            uiInterface.InitMessage(_("Pruning blockstore..."));
            PruneAndFlush();
        }
    }

    // Step 11: import blocks
    if (!CheckDiskSpace(GetDataDir())) {
        InitError(
            strprintf(_("Error: Disk space is low for %s"), GetDataDir()));
        return false;
    }
    if (!CheckDiskSpace(GetBlocksDir())) {
        InitError(
            strprintf(_("Error: Disk space is low for %s"), GetBlocksDir()));
        return false;
    }
    if (!CheckDiskSpace(GetIndexDir())) {
        InitError(
            strprintf(_("Error: Disk space is low for %s"), GetIndexDir()));
        return false;
    }

    // Either install a handler to notify us when genesis activates, or set
    // fHaveGenesis directly.
    // No locking, as this happens before any background thread is started.
    if (::ChainActive().Tip() == nullptr) {
        uiInterface.NotifyBlockTip_connect(BlockNotifyGenesisWait);
    } else {
        fHaveGenesis = true;
    }

    if (gArgs.IsArgSet("-blocknotify")) {
        uiInterface.NotifyBlockTip_connect(BlockNotifyCallback);
    }

    std::vector<fs::path> vImportFiles;
    for (const std::string &strFile : gArgs.GetArgs("-loadblock")) {
        vImportFiles.push_back(strFile);
    }

    threadGroup.create_thread(
        std::bind(&ThreadImport, std::ref(config), vImportFiles));

    // Wait for genesis block to be processed
    {
        WAIT_LOCK(g_genesis_wait_mutex, lock);
        // We previously could hang here if StartShutdown() is called prior to
        // ThreadImport getting started, so instead we just wait on a timer to
        // check ShutdownRequested() regularly.
        while (!fHaveGenesis && !ShutdownRequested()) {
            g_genesis_wait_cv.wait_for(lock, std::chrono::milliseconds(500));
        }
        uiInterface.NotifyBlockTip_disconnect(BlockNotifyGenesisWait);
    }

    if (ShutdownRequested()) {
        return false;
    }

    // Step 12: start node

    //// Ensure g_best_block (used by mining RPC) is initialized
    {
        LOCK2(cs_main, g_best_block_mutex);
        if (auto *tip = ::ChainActive().Tip()) {
            g_best_block = tip->GetBlockHash();
        }
    }

    int chain_active_height;

    //// debug print
    {
        LOCK(cs_main);
        LogPrintf("mapBlockIndex.size() = %u\n", mapBlockIndex.size());
        chain_active_height = ::ChainActive().Height();
    }
    LogPrintf("nBestHeight = %d\n", chain_active_height);

    Discover();

    // Map ports with UPnP
    if (gArgs.GetBoolArg("-upnp", DEFAULT_UPNP)) {
        StartMapPort();
    }

    CConnman::Options connOptions;
    connOptions.nLocalServices = nLocalServices;
    connOptions.nMaxConnections = nMaxConnections;
    connOptions.nMaxOutbound =
        std::min(MAX_OUTBOUND_CONNECTIONS, connOptions.nMaxConnections);
    connOptions.nMaxAddnode = MAX_ADDNODE_CONNECTIONS;
    connOptions.nMaxFeeler = 1;
    connOptions.nBestHeight = chain_active_height;
    connOptions.uiInterface = &uiInterface;
    connOptions.m_banman = g_banman.get();
    connOptions.m_msgproc = peerLogic.get();
    connOptions.nSendBufferMaxSize =
        1000 * gArgs.GetArg("-maxsendbuffer", DEFAULT_MAXSENDBUFFER);
    connOptions.nReceiveFloodSize =
        1000 * gArgs.GetArg("-maxreceivebuffer", DEFAULT_MAXRECEIVEBUFFER);
    connOptions.m_added_nodes = gArgs.GetArgs("-addnode");

    connOptions.nMaxOutboundTimeframe = nMaxOutboundTimeframe;
    connOptions.nMaxOutboundLimit = nMaxOutboundLimit;
    connOptions.m_peer_connect_timeout = peer_connect_timeout;

    for (const std::string &bind_arg : gArgs.GetArgs("-bind")) {
        CService bind_addr;
        const size_t index = bind_arg.rfind('=');
        if (index == std::string::npos) {
            if (Lookup(bind_arg, bind_addr, GetListenPort(), false)) {
                connOptions.vBinds.push_back(bind_addr);
                continue;
            }
        } else {
            const std::string network_type = bind_arg.substr(index + 1);
            if (network_type == "onion") {
                const std::string truncated_bind_arg = bind_arg.substr(0, index);
                if (Lookup(truncated_bind_arg, bind_addr, BaseParams().OnionServiceTargetPort(), false)) {
                    connOptions.onion_binds.push_back(bind_addr);
                    continue;
                }
            }
        }
        return InitError(ResolveErrMsg("bind", bind_arg));
    }

    if (connOptions.onion_binds.empty()) {
        connOptions.onion_binds.push_back(DefaultOnionServiceTarget());
    }

    if (gArgs.GetBoolArg("-listenonion", DEFAULT_LISTEN_ONION)) {
        const auto bind_addr = connOptions.onion_binds.front();
        if (connOptions.onion_binds.size() > 1) {
            InitWarning(strprintf(_("More than one onion bind address is provided. Using %s for the automatically created Tor onion service."), bind_addr.ToStringIPPort()));
        }
        StartTorControl(bind_addr);
    }

    for (const std::string &strBind : gArgs.GetArgs("-whitebind")) {
        NetWhitebindPermissions whitebind;
        std::string error;
        if (!NetWhitebindPermissions::TryParse(strBind, whitebind, error)) {
            return InitError(error);
        }
        connOptions.vWhiteBinds.push_back(whitebind);
    }

    for (const auto &net : gArgs.GetArgs("-whitelist")) {
        NetWhitelistPermissions subnet;
        std::string error;
        if (!NetWhitelistPermissions::TryParse(net, subnet, error)) {
            return InitError(error);
        }
        connOptions.vWhitelistedRange.push_back(subnet);
    }

    connOptions.vSeedNodes = gArgs.GetArgs("-seednode");

    // Initiate outbound connections unless connect=0
    connOptions.m_use_addrman_outgoing = !gArgs.IsArgSet("-connect");
    if (!connOptions.m_use_addrman_outgoing) {
        const auto connect = gArgs.GetArgs("-connect");
        if (connect.size() != 1 || connect[0] != "0") {
            connOptions.m_specified_outgoing = connect;
        }
    }
    if (!g_connman->Start(scheduler, connOptions)) {
        return false;
    }

    // Step 13: finished

    SetRPCWarmupFinished();
    uiInterface.InitMessage(_("Done loading"));

    for (const auto &client : node.chain_clients) {
        client->start(scheduler);
    }

    scheduler.scheduleEvery(
        [] {
            g_banman->DumpBanlist();
            return true;
        },
        DUMP_BANS_INTERVAL * 1000);

    return true;
}
