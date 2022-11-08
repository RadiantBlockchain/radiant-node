// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <chainparams.h>
#include <clientversion.h>
#include <compat.h>
#include <config.h>
#include <fs.h>
#include <httprpc.h>
#include <httpserver.h>
#include <init.h>
#include <interfaces/chain.h>
#include <noui.h>
#include <rpc/server.h>
#include <shutdown.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <util/threadnames.h>
#include <validationinterface.h>
#include <walletinitinterface.h>

#include <cstdio>

const std::function<std::string(const char *)> G_TRANSLATION_FUN = nullptr;

/* Introduction text for doxygen: */

/*! \mainpage Developer documentation
 *
 * \section intro_sec Introduction
 *
 * This is the developer documentation of Radiant Node
 * (https://radiantblockchain.org/). Radiant Node is a client for the
 * blockchain platform called Radiant (https://radiantblockchain.org/), which
 * enables instant payments to anyone, anywhere in the world. Radiant uses
 * peer-to-peer technology to operate with no central authority: managing
 * transactions and issuing money are carried out collectively by the network.
 *
 * The software is a community-driven open source project, released under the
 * Radiant MIT license.
 *
 * \section Navigation
 * Use the buttons <code>Namespaces</code>, <code>Classes</code> or
 * <code>Files</code> at the top of the page to start navigating the code.
 */

static void WaitForShutdown() {
    while (!ShutdownRequested()) {
        MilliSleep(200);
    }
    Interrupt();
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
static bool AppInit(int argc, char *argv[]) {
    // FIXME: Ideally, we'd like to build the config here, but that's currently
    // not possible as the whole application has too much global state. However,
    // this is a first step.
    auto &config = const_cast<Config &>(GetConfig());
    RPCServer rpcServer;
    HTTPRPCRequestProcessor httpRPCRequestProcessor(config, rpcServer);

    NodeContext node;
    node.chain = interfaces::MakeChain();

    bool fRet = false;

    util::ThreadSetInternalName("init");

    //
    // Parameters
    //
    // If Qt is used, parameters/bitcoin.conf are parsed in qt/bitcoin.cpp's
    // main()
    SetupServerArgs();
    std::string error;
    if (!gArgs.ParseParameters(argc, argv, error)) {
        fprintf(stderr, "Error parsing command line arguments: %s\n",
                error.c_str());
        return false;
    }

    // Process help and version before taking care about datadir
    if (HelpRequested(gArgs) || gArgs.IsArgSet("-version")) {
        std::string strUsage =
            PACKAGE_NAME " Daemon version " + FormatFullVersion() + "\n";

        if (gArgs.IsArgSet("-version")) {
            strUsage += FormatParagraph(LicenseInfo());
        } else {
            strUsage += "\nUsage:  bitcoind [options]                     "
                        "Start " PACKAGE_NAME " Daemon\n";

            strUsage += "\n" + gArgs.GetHelpMessage();
        }

        fprintf(stdout, "%s", strUsage.c_str());
        return true;
    }

    try {
        if (!fs::is_directory(GetDataDir(false))) {
            fprintf(stderr,
                    "Error: Specified data directory \"%s\" does not exist.\n",
                    gArgs.GetArg("-datadir", "").c_str());
            return false;
        }
        if (!gArgs.ReadConfigFiles(error)) {
            fprintf(stderr, "Error reading configuration file: %s\n",
                    error.c_str());
            return false;
        }
        // Check for -testnet or -regtest parameter (Params() calls are only
        // valid after this clause)
        try {
            SelectParams(gArgs.GetChainName());
        } catch (const std::exception &e) {
            fprintf(stderr, "Error: %s\n", e.what());
            return false;
        }

        // Make sure we create the net-specific data directory early on: if it
        // is new, this has a side effect of also creating
        // <datadir>/<net>/wallets/.
        //
        // TODO: this should be removed once GetDataDir() no longer creates the
        // wallets/ subdirectory.
        // See more info at:
        // https://reviews.bitcoinabc.org/D3312
        GetDataDir(true);

        // Error out when loose non-argument tokens are encountered on command
        // line
        for (int i = 1; i < argc; i++) {
            if (!IsSwitchChar(argv[i][0])) {
                fprintf(stderr,
                        "Error: Command line contains unexpected token '%s', "
                        "see radiantd -h for a list of options.\n",
                        argv[i]);
                return false;
            }
        }

        // -server defaults to true for bitcoind but not for the GUI so do this
        // here
        gArgs.SoftSetBoolArg("-server", true);
        // Set this early so that parameter interactions go to console
        InitLogging();
        InitParameterInteraction();
        if (!AppInitBasicSetup()) {
            // InitError will have been called with detailed error, which ends
            // up on console
            return false;
        }
        if (!AppInitParameterInteraction(config)) {
            // InitError will have been called with detailed error, which ends
            // up on console
            return false;
        }
        if (!AppInitSanityChecks()) {
            // InitError will have been called with detailed error, which ends
            // up on console
            return false;
        }
        if (gArgs.GetBoolArg("-daemon", false)) {
#if HAVE_DECL_DAEMON
#if defined(MAC_OSX)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
            fprintf(stdout, "Radiant server starting\n");

            // Daemonize
            if (daemon(1, 0)) {
                // don't chdir (1), do close FDs (0)
                fprintf(stderr, "Error: daemon() failed: %s\n",
                        strerror(errno));
                return false;
            }
#if defined(MAC_OSX)
#pragma GCC diagnostic pop
#endif
#else
            fprintf(
                stderr,
                "Error: -daemon is not supported on this operating system\n");
            return false;
#endif // HAVE_DECL_DAEMON
        }

        // Lock data directory after daemonization
        if (!AppInitLockDataDirectory()) {
            // If locking the data directory failed, exit immediately
            return false;
        }
        fRet = AppInitMain(config, rpcServer, httpRPCRequestProcessor, node);
    } catch (const std::exception &e) {
        PrintExceptionContinue(&e, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInit()");
    }

    if (!fRet) {
        Interrupt();
    } else {
        SetValidationInterfaceRegistrationsUnsafe(true);
        WaitForShutdown();
        SetValidationInterfaceRegistrationsUnsafe(false);
    }
    Shutdown(node);

    return fRet;
}

int main(int argc, char *argv[]) {
#ifdef WIN32
    util::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif
    SetupEnvironment();

    // Connect bitcoind signal handlers
    noui_connect();

    return (AppInit(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE);
}
