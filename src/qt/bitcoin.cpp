// Copyright (c) 2011-2019 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/bitcoin.h>

#include <chainparams.h>
#include <config.h>
#include <fs.h>
#include <httprpc.h>
#include <init.h> // LicenseInfo
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <noui.h>
#include <qt/bitcoingui.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/intro.h>
#include <qt/networkstyle.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/splashscreen.h>
#include <qt/utilitydialog.h>
#include <qt/winshutdownmonitor.h>
#include <rpc/server.h>
#include <ui_interface.h>
#include <uint256.h>
#include <util/strencodings.h> // FormatParagraph
#include <util/system.h>
#include <util/threadnames.h>
#include <walletinitinterface.h>
#include <warnings.h>

#ifdef ENABLE_WALLET
#include <qt/paymentserver.h>
#include <qt/walletcontroller.h>
#endif

#include <QApplication>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSettings>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QTranslator>

#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
#if defined(QT_QPA_PLATFORM_XCB)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_WINDOWS)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_COCOA)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#endif
#endif

#include <cstdint>
#include <memory>

/** Default for -min */
static constexpr bool DEFAULT_START_MINIMIZED = false;

// Declare meta types used for QMetaObject::invokeMethod
Q_DECLARE_METATYPE(bool *)
Q_DECLARE_METATYPE(Amount)
Q_DECLARE_METATYPE(uint256)

// Config is non-copyable so we can only register pointers to it
Q_DECLARE_METATYPE(Config *)

static QString GetLangTerritory() {
    QSettings settings;
    // Get desired locale (e.g. "de_DE")
    // 1) System default language
    QString lang_territory = QLocale::system().name();
    // 2) Language from QSettings
    QString lang_territory_qsettings =
        settings.value("language", "").toString();
    if (!lang_territory_qsettings.isEmpty()) {
        lang_territory = lang_territory_qsettings;
    }
    // 3) -lang command line argument
    lang_territory = QString::fromStdString(
        gArgs.GetArg("-lang", lang_territory.toStdString()));
    return lang_territory;
}

/** Set up translations */
static void initTranslations(QTranslator &qtTranslatorBase,
                             QTranslator &qtTranslator,
                             QTranslator &translatorBase,
                             QTranslator &translator) {
    // Remove old translators
    QApplication::removeTranslator(&qtTranslatorBase);
    QApplication::removeTranslator(&qtTranslator);
    QApplication::removeTranslator(&translatorBase);
    QApplication::removeTranslator(&translator);

    // Get desired locale (e.g. "de_DE")
    QString lang_territory = GetLangTerritory();

    // Set Qt's global locale, e.g. to get correct number notation.
    QLocale::setDefault(QLocale(lang_territory));

    // Convert to "de" only by truncating "_DE"
    QString lang = lang_territory;
    lang.truncate(lang_territory.lastIndexOf('_'));

    // Load language files for configured locale:
    // - First load the translator for the base language, without territory
    // - Then load the more specific locale translator

    // Load e.g. qt_de.qm
    if (qtTranslatorBase.load(
            "qt_" + lang,
            QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        QApplication::installTranslator(&qtTranslatorBase);
    }

    // Load e.g. qt_de_DE.qm
    if (qtTranslator.load(
            "qt_" + lang_territory,
            QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        QApplication::installTranslator(&qtTranslator);
    }

    // Load e.g. bitcoin_de.qm (shortcut "de" needs to be defined in
    // bitcoin.qrc)
    if (translatorBase.load(lang, ":/translations/")) {
        QApplication::installTranslator(&translatorBase);
    }

    // Load e.g. bitcoin_de_DE.qm (shortcut "de_DE" needs to be defined in
    // bitcoin.qrc)
    if (translator.load(lang_territory, ":/translations/")) {
        QApplication::installTranslator(&translator);
    }
}

/* qDebug() message handler --> debug.log */
void DebugMessageHandler(QtMsgType type, const QMessageLogContext &context,
                         const QString &msg) {
    Q_UNUSED(context);
    if (type == QtDebugMsg) {
        LogPrint(BCLog::QT, "GUI: %s\n", msg.toStdString());
    } else {
        LogPrintf("GUI: %s\n", msg.toStdString());
    }
}

BitcoinCashNode::BitcoinCashNode(interfaces::Node &node) : QObject(), m_node(node) {}

void BitcoinCashNode::handleRunawayException(const std::exception *e) {
    PrintExceptionContinue(e, "Runaway exception");
    Q_EMIT runawayException(QString::fromStdString(m_node.getWarnings("gui")));
}

void BitcoinCashNode::initialize(Config *config, RPCServer *rpcServer,
                            HTTPRPCRequestProcessor *httpRPCRequestProcessor) {
    try {
        qDebug() << __func__ << ": Running initialization in thread";
        util::ThreadRename("qt-init");
        bool rv =
            m_node.appInitMain(*config, *rpcServer, *httpRPCRequestProcessor);
        Q_EMIT initializeResult(rv);
    } catch (const std::exception &e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(nullptr);
    }
}

void BitcoinCashNode::shutdown() {
    try {
        qDebug() << __func__ << ": Running Shutdown in thread";
        m_node.appShutdown();
        qDebug() << __func__ << ": Shutdown finished";
        Q_EMIT shutdownResult();
    } catch (const std::exception &e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(nullptr);
    }
}

BitcoinApplication::BitcoinApplication(interfaces::Node &node, int &argc,
                                       char **argv)
    : QApplication(argc, argv), coreThread(nullptr), m_node(node),
      optionsModel(nullptr), clientModel(nullptr), window(nullptr),
      pollShutdownTimer(nullptr), returnValue(0), platformStyle(nullptr) {
    setQuitOnLastWindowClosed(false);
}

void BitcoinApplication::setupPlatformStyle() {
    // UI per-platform customization
    // This must be done inside the BitcoinApplication constructor, or after it,
    // because PlatformStyle::instantiate requires a QApplication.
    std::string platformName;
    platformName = gArgs.GetArg("-uiplatform", BitcoinGUI::DEFAULT_UIPLATFORM);
    platformStyle =
        PlatformStyle::instantiate(QString::fromStdString(platformName));
    // Fall back to "other" if specified name not found.
    if (!platformStyle) {
        platformStyle = PlatformStyle::instantiate("other");
    }
    assert(platformStyle);
}

BitcoinApplication::~BitcoinApplication() {
    if (coreThread) {
        qDebug() << __func__ << ": Stopping thread";
        Q_EMIT stopThread();
        coreThread->wait();
        qDebug() << __func__ << ": Stopped thread";
    }

    delete window;
    window = nullptr;
#ifdef ENABLE_WALLET
    delete paymentServer;
    paymentServer = nullptr;
    delete m_wallet_controller;
    m_wallet_controller = nullptr;
#endif
    delete optionsModel;
    optionsModel = nullptr;
    delete platformStyle;
    platformStyle = nullptr;
}

#ifdef ENABLE_WALLET
void BitcoinApplication::createPaymentServer() {
    paymentServer = new PaymentServer(this);
}
#endif

void BitcoinApplication::createOptionsModel(bool resetSettings) {
    optionsModel = new OptionsModel(m_node, nullptr, resetSettings);
}

void BitcoinApplication::createWindow(const Config *config,
                                      const NetworkStyle *networkStyle) {
    window =
        new BitcoinGUI(m_node, config, platformStyle, networkStyle, nullptr);

    pollShutdownTimer = new QTimer(window);
    connect(pollShutdownTimer, &QTimer::timeout, window,
            &BitcoinGUI::detectShutdown);
}

void BitcoinApplication::createSplashScreen(const NetworkStyle *networkStyle) {
    SplashScreen *splash = new SplashScreen(m_node, networkStyle);
    // We don't hold a direct pointer to the splash screen after creation, but
    // the splash screen will take care of deleting itself when slotFinish
    // happens.
    splash->show();
    connect(this, &BitcoinApplication::splashFinished, splash,
            &SplashScreen::slotFinish);
    connect(this, &BitcoinApplication::requestedShutdown, splash,
            &QWidget::close);
}

bool BitcoinApplication::baseInitialize(Config &config) {
    return m_node.baseInitialize(config);
}

void BitcoinApplication::startThread() {
    if (coreThread) {
        return;
    }
    coreThread = new QThread(this);
    BitcoinCashNode *executor = new BitcoinCashNode(m_node);
    executor->moveToThread(coreThread);

    /*  communication to and from thread */
    connect(executor, &BitcoinCashNode::initializeResult, this,
            &BitcoinApplication::initializeResult);
    connect(executor, &BitcoinCashNode::shutdownResult, this,
            &BitcoinApplication::shutdownResult);
    connect(executor, &BitcoinCashNode::runawayException, this,
            &BitcoinApplication::handleRunawayException);

    // Note on how Qt works: it tries to directly invoke methods if the signal
    // is emitted on the same thread that the target object 'lives' on.
    // But if the target object 'lives' on another thread (executor here does)
    // the SLOT will be invoked asynchronously at a later time in the thread
    // of the target object.  So.. we pass a pointer around.  If you pass
    // a reference around (even if it's non-const) you'll get Qt generating
    // code to copy-construct the parameter in question (Q_DECLARE_METATYPE
    // and qRegisterMetaType generate this code).  For the Config class,
    // which is noncopyable, we can't do this.  So.. we have to pass
    // pointers to Config around.  Make sure Config &/Config * isn't a
    // temporary (eg it lives somewhere aside from the stack) or this will
    // crash because initialize() gets executed in another thread at some
    // unspecified time (after) requestedInitialize() is emitted!
    connect(this, &BitcoinApplication::requestedInitialize, executor,
            &BitcoinCashNode::initialize);

    connect(this, &BitcoinApplication::requestedShutdown, executor,
            &BitcoinCashNode::shutdown);
    /*  make sure executor object is deleted in its own thread */
    connect(this, &BitcoinApplication::stopThread, executor,
            &QObject::deleteLater);
    connect(this, &BitcoinApplication::stopThread, coreThread, &QThread::quit);

    coreThread->start();
}

void BitcoinApplication::parameterSetup() {
    // Default printtoconsole to false for the GUI. GUI programs should not
    // print to the console unnecessarily.
    gArgs.SoftSetBoolArg("-printtoconsole", false);

    m_node.initLogging();
    m_node.initParameterInteraction();
}

void BitcoinApplication::requestInitialize(
    Config &config, RPCServer &rpcServer,
    HTTPRPCRequestProcessor &httpRPCRequestProcessor) {
    qDebug() << __func__ << ": Requesting initialize";
    startThread();
    // IMPORTANT: config must NOT be a reference to a temporary because below
    // signal may be connected to a slot that will be executed as a queued
    // connection in another thread!
    Q_EMIT requestedInitialize(&config, &rpcServer, &httpRPCRequestProcessor);
}

void BitcoinApplication::requestShutdown(Config &config) {
    // Show a simple window indicating shutdown status. Do this first as some of
    // the steps may take some time below, for example the RPC console may still
    // be executing a command.
    shutdownWindow.reset(ShutdownWindow::showShutdownWindow(window));

    qDebug() << __func__ << ": Requesting shutdown";
    startThread();
    window->hide();
    // Must disconnect node signals otherwise current thread can deadlock since
    // no event loop is running.
    window->unsubscribeFromCoreSignals();
    // Request node shutdown, which can interrupt long operations, like
    // rescanning a wallet.
    m_node.startShutdown();
    // Unsetting the client model can cause the current thread to wait for node
    // to complete an operation, like wait for a RPC execution to complate.
    window->setClientModel(nullptr);
    pollShutdownTimer->stop();

    delete clientModel;
    clientModel = nullptr;

    // Request shutdown from core thread
    Q_EMIT requestedShutdown();
}

void BitcoinApplication::initializeResult(bool success) {
    qDebug() << __func__ << ": Initialization result: " << success;
    returnValue = success ? EXIT_SUCCESS : EXIT_FAILURE;
    if (!success) {
        // Make sure splash screen doesn't stick around during shutdown.
        Q_EMIT splashFinished(window);
        // Exit first main loop invocation.
        quit();
        return;
    }
    // Log this only after AppInitMain finishes, as then logging setup is
    // guaranteed complete.
    qWarning() << "Platform customization:" << platformStyle->getName();
#ifdef ENABLE_WALLET
    m_wallet_controller =
        new WalletController(m_node, platformStyle, optionsModel, this);
#ifdef ENABLE_BIP70
    PaymentServer::LoadRootCAs();
#endif
    if (paymentServer) {
        paymentServer->setOptionsModel(optionsModel);
#ifdef ENABLE_BIP70
        connect(m_wallet_controller, &WalletController::coinsSent,
                paymentServer, &PaymentServer::fetchPaymentACK);
#endif
    }
#endif

    clientModel = new ClientModel(m_node, optionsModel);
    window->setClientModel(clientModel);
#ifdef ENABLE_WALLET
    window->setWalletController(m_wallet_controller);
#endif

    // If -min option passed, start window minimized.
    if (gArgs.GetBoolArg("-min", DEFAULT_START_MINIMIZED)) {
        window->showMinimized();
    } else {
        window->show();
    }
    Q_EMIT splashFinished(window);
    Q_EMIT windowShown(window);

#ifdef ENABLE_WALLET
    // Now that initialization/startup is done, process any command-line
    // bitcoincash: URIs or payment requests:
    if (paymentServer) {
        connect(paymentServer, &PaymentServer::receivedPaymentRequest, window,
                &BitcoinGUI::handlePaymentRequest);
        connect(window, &BitcoinGUI::receivedURI, paymentServer,
                &PaymentServer::handleURIOrFile);
        connect(paymentServer, &PaymentServer::message,
                [this](const QString &title, const QString &message,
                       unsigned int style) {
                    window->message(title, message, style);
                });
        QTimer::singleShot(100, paymentServer, &PaymentServer::uiReady);
    }
#endif

    pollShutdownTimer->start(200);
}

void BitcoinApplication::shutdownResult() {
    // Exit second main loop invocation after shutdown finished.
    quit();
}

void BitcoinApplication::handleRunawayException(const QString &message) {
    QMessageBox::critical(
        nullptr, "Runaway exception",
        BitcoinGUI::tr("A fatal error occurred. Bitcoin can no longer continue "
                       "safely and will quit.") +
            QString("\n\n") + message);
    ::exit(EXIT_FAILURE);
}

WId BitcoinApplication::getMainWinId() const {
    if (!window) {
        return 0;
    }

    return window->winId();
}

static void SetupUIArgs() {
#if defined(ENABLE_WALLET) && defined(ENABLE_BIP70)
    gArgs.AddArg("-allowselfsignedrootcertificates",
                 strprintf("Allow self signed root certificates (default: %d)",
                           DEFAULT_SELFSIGNED_ROOTCERTS),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::GUI);
#endif
    gArgs.AddArg("-choosedatadir",
                 strprintf("Choose data directory on startup (default: %d)",
                           DEFAULT_CHOOSE_DATADIR),
                 ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    gArgs.AddArg("-lang=<lang>",
                 "Set language, for example \"de_DE\" (default: system locale)",
                 ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    gArgs.AddArg("-min", strprintf("Start minimized (default: %d)", DEFAULT_START_MINIMIZED), ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    gArgs.AddArg(
        "-rootcertificates=<file>",
        "Set SSL root certificates for payment request (default: -system-)",
        ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    gArgs.AddArg("-splash",
                 strprintf("Show splash screen on startup (default: %d)",
                           DEFAULT_SPLASHSCREEN),
                 ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    gArgs.AddArg("-resetguisettings", "Reset all settings changed in the GUI",
                 ArgsManager::ALLOW_ANY, OptionsCategory::GUI);
    gArgs.AddArg("-uiplatform",
                 strprintf("Select platform to customize UI for (one of "
                           "windows, macosx, other; default: %s)",
                           BitcoinGUI::DEFAULT_UIPLATFORM),
                 ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::GUI);
}

#ifndef BITCOIN_QT_TEST

int GuiMain(int argc, char *argv[]) {
#ifdef WIN32
    util::WinCmdLineArgs winArgs;
    std::tie(argc, argv) = winArgs.get();
#endif
    SetupEnvironment();
    util::ThreadSetInternalName("main");

    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    // Subscribe to global signals from core
    std::unique_ptr<interfaces::Handler> handler_message_box =
        node->handleMessageBox(noui_ThreadSafeMessageBox);
    std::unique_ptr<interfaces::Handler> handler_question =
        node->handleQuestion(noui_ThreadSafeQuestion);
    std::unique_ptr<interfaces::Handler> handler_init_message =
        node->handleInitMessage(noui_InitMessage);

    // Do not refer to data directory yet, this can be overridden by
    // Intro::pickDataDirectory

    /// 0. Parse bitcoin-qt command-line options.
    // Command-line options take precedence:
    node->setupServerArgs();
    SetupUIArgs();
    std::string error;
    const bool parametersParsed = node->parseParameters(argc, argv, error);

    const bool versionRequested = parametersParsed && gArgs.IsArgSet("-version");
    const bool versionOrHelpRequested = versionRequested || (parametersParsed && HelpRequested(gArgs));

#if !defined(WIN32)
    // On non-Windows operating systems, print help text to console.
    // We intentionally do this before loading Qt, so that this also works on platforms without display.
    if (versionOrHelpRequested) {
        fprintf(stdout, "%s\n", HelpMessageDialog::versionText().toStdString().c_str());
        if (versionRequested) {
            fprintf(stdout, "%s", FormatParagraph(LicenseInfo()).c_str());
        } else {
            fprintf(stdout, "\n%s\n%s", HelpMessageDialog::headerText, gArgs.GetHelpMessage().c_str());
        }
        return EXIT_SUCCESS;
    }
#endif

    /// 1. Basic Qt initialization (not dependent on parameters or
    /// configuration)
    Q_INIT_RESOURCE(bitcoin);
    Q_INIT_RESOURCE(bitcoin_locale);

#if QT_VERSION >= 0x050600
    // Note this must be set *before* the QApplication is constructed.
    // See: https://doc.qt.io/qt-5/qt.html#ApplicationAttribute-enum
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    // Command-line arguments were already parsed above, so Qt can no longer extract Qt-internal arguments: these would already be
    // deemed invalid arguments above. Hence hide the command-line arguments from QtApplication. If one nevertheless needs to use
    // some Qt-internal arguments, they should be made available by wrapping them in arguments defined in gArgs (which also ensures
    // syntax consistency and yields visibility in documentation).
    int argcQt = 1;
    BitcoinApplication app(*node, argcQt, argv);
#if QT_VERSION > 0x050100
    // Generate high-dpi pixmaps
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#ifdef Q_OS_MAC
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    // Register meta types used for QMetaObject::invokeMethod
    qRegisterMetaType<bool *>();
    //   Need to pass name here as Amount is a typedef (see
    //   http://qt-project.org/doc/qt-5/qmetatype.html#qRegisterMetaType)
    //   IMPORTANT if it is no longer a typedef use the normal variant above
    qRegisterMetaType<Amount>("Amount");
    qRegisterMetaType<std::function<void()>>("std::function<void()>");

    // Need to register any types Qt doesn't know about if you intend
    // to use them with the signal/slot mechanism Qt provides. Even pointers.
    // Note that class Config is noncopyable and so we can't register a
    // non-pointer version of it with Qt, because Qt expects to be able to
    // copy-construct non-pointers to objects for invoking slots
    // behind-the-scenes in the 'Queued' connection case.
    qRegisterMetaType<Config *>();

    /// 2. Show any error parsing parameters. Now that Qt is initialized, we can show the error.
    if (!parametersParsed) {
        QMessageBox::critical(
            nullptr, PACKAGE_NAME,
            QObject::tr("Error parsing command line arguments: %1.")
                .arg(QString::fromStdString(error)));
        return EXIT_FAILURE;
    }

    // Now that the QApplication is setup and we have parsed our parameters, we
    // can set the platform style
    app.setupPlatformStyle();

    /// 3. Application identification
    // must be set before OptionsModel is initialized or translations are
    // loaded, as it is used to locate QSettings.
    QApplication::setOrganizationName(QAPP_ORG_NAME);
    QApplication::setOrganizationDomain(QAPP_ORG_DOMAIN);
    QApplication::setApplicationName(QAPP_APP_NAME_DEFAULT);

    /// 4. Initialization of translations, so that intro dialog is in user's
    /// language. Now that QSettings are accessible, initialize translations.
    QTranslator qtTranslatorBase, qtTranslator, translatorBase, translator;
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase,
                     translator);

#if defined(WIN32)
    // On Windows, show a message box, as there is no stderr/stdout in windowed
    // applications. Do so immediately after parsing command-line options (for
    // "-lang") and setting locale, but before showing splash screen.
    if (versionOrHelpRequested) {
        HelpMessageDialog help(*node, nullptr, versionRequested);
        help.exec();
        return EXIT_SUCCESS;
    }
#endif

    /// 5. Now that settings and translations are available, ask user for data
    /// directory. User language is set up: pick a data directory.
    if (!Intro::pickDataDirectory(*node)) {
        return EXIT_SUCCESS;
    }

    /// 6. Determine availability of data and blocks directory and parse
    /// radiant.conf
    /// - Do not call GetDataDir(true) before this step finishes.
    if (!fs::is_directory(GetDataDir(false))) {
        QMessageBox::critical(
            nullptr, PACKAGE_NAME,
            QObject::tr(
                "Error: Specified data directory \"%1\" does not exist.")
                .arg(QString::fromStdString(gArgs.GetArg("-datadir", ""))));
        return EXIT_FAILURE;
    }
    if (!node->readConfigFiles(error)) {
        QMessageBox::critical(
            nullptr, PACKAGE_NAME,
            QObject::tr("Error: Cannot parse configuration file: %1.")
                .arg(QString::fromStdString(error)));
        return EXIT_FAILURE;
    }

    /// 7. Determine network (and switch to network specific options)
    // - Do not call Params() before this step.
    // - Do this after parsing the configuration file, as the network can be
    // switched there.
    // - QSettings() will use the new application name after this, resulting in
    // network-specific settings.
    // - Needs to be done before createOptionsModel.

    // Check for -testnet or -regtest parameter (Params() calls are only valid
    // after this clause)
    try {
        node->selectParams(gArgs.GetChainName());
    } catch (std::exception &e) {
        QMessageBox::critical(nullptr, PACKAGE_NAME,
                              QObject::tr("Error: %1").arg(e.what()));
        return EXIT_FAILURE;
    }
#ifdef ENABLE_WALLET
    // Parse URIs on command line -- this can affect Params()
    PaymentServer::ipcParseCommandLine(*node, argc, argv);
#endif

    QScopedPointer<const NetworkStyle> networkStyle(NetworkStyle::instantiate(
        QString::fromStdString(Params().NetworkIDString())));
    assert(!networkStyle.isNull());
    // Allow for separate UI settings for testnets
    QApplication::setApplicationName(networkStyle->getAppName());
    // Re-initialize translations after changing application name (language in
    // network-specific settings can be different)
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase,
                     translator);

#ifdef ENABLE_WALLET
    /// 8. URI IPC sending
    // - Do this early as we don't want to bother initializing if we are just
    // calling IPC
    // - Do this *after* setting up the data directory, as the data directory
    // hash is used in the name
    // of the server.
    // - Do this after creating app and setting up translations, so errors are
    // translated properly.
    if (PaymentServer::ipcSendCommandLine()) {
        exit(EXIT_SUCCESS);
    }

    // Start up the payment server early, too, so impatient users that click on
    // bitcoincash: links repeatedly have their payment requests routed to this
    // process:
    app.createPaymentServer();
#endif

    /// 9. Main GUI initialization
    // Install global event filter that makes sure that long tooltips can be
    // word-wrapped.
    app.installEventFilter(
        new GUIUtil::ToolTipToRichTextFilter(TOOLTIP_WRAP_THRESHOLD, &app));
#if defined(Q_OS_WIN)
    // Install global event filter for processing Windows session related
    // Windows messages (WM_QUERYENDSESSION and WM_ENDSESSION)
    qApp->installNativeEventFilter(new WinShutdownMonitor());
#endif
    // Install qDebug() message handler to route to debug.log
    qInstallMessageHandler(DebugMessageHandler);
    // Allow parameter interaction before we create the options model
    app.parameterSetup();
    // Load GUI settings from QSettings
    app.createOptionsModel(gArgs.GetBoolArg("-resetguisettings", false));

    // Get global config
    Config &config = const_cast<Config &>(GetConfig());

    if (gArgs.GetBoolArg("-splash", DEFAULT_SPLASHSCREEN) &&
        !gArgs.GetBoolArg("-min", DEFAULT_START_MINIMIZED)) {
        app.createSplashScreen(networkStyle.data());
    }

    RPCServer rpcServer;
    HTTPRPCRequestProcessor httpRPCRequestProcessor(config, rpcServer);

    try {
        app.createWindow(&config, networkStyle.data());
        // Perform base initialization before spinning up
        // initialization/shutdown thread. This is acceptable because this
        // function only contains steps that are quick to execute, so the GUI
        // thread won't be held up.
        if (!app.baseInitialize(config)) {
            // A dialog with detailed error will have been shown by InitError()
            return EXIT_FAILURE;
        }
        app.requestInitialize(config, rpcServer, httpRPCRequestProcessor);
#if defined(Q_OS_WIN)
        WinShutdownMonitor::registerShutdownBlockReason(
            QObject::tr("%1 didn't yet exit safely...")
                .arg(PACKAGE_NAME),
            (HWND)app.getMainWinId());
#endif
        app.exec();
        app.requestShutdown(config);
        app.exec();
        return app.getReturnValue();
    } catch (const std::exception &e) {
        PrintExceptionContinue(&e, "Runaway exception");
        app.handleRunawayException(
            QString::fromStdString(node->getWarnings("gui")));
    } catch (...) {
        PrintExceptionContinue(nullptr, "Runaway exception");
        app.handleRunawayException(
            QString::fromStdString(node->getWarnings("gui")));
    }
    return EXIT_FAILURE;
}
#endif // BITCOIN_QT_TEST
