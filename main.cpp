#ifdef Q_OS_WIN
    #include <DbgHelp.h>
    #include <Windows.h>
#endif

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QLockFile>
#include <QStandardPaths>

#include <csignal>
#include <execinfo.h>

#include "console/console_manager.h"
#include "datastorage/dfs/dfs_controller.h"
#include "managers/extrachain_node.h"
#include "managers/logs_manager.h"
#include "metatypes.h"

#ifndef EXTRACHAIN_CMAKE
    #include "preconfig.h"
#endif

#ifdef Q_OS_WIN
const char* strsignal(int sig) {
    static const std::unordered_map<int, const char*> sig2NameMap = {
        { SIGINT, "SIGINT" }, { SIGTERM, "SIGTERM" }, { SIGABRT, "SIGABRT" }, { SIGSEGV, "SIGSEGV" }
    };

    const auto iter = sig2NameMap.find(sig);
    if (iter != sig2NameMap.end())
        return iter->second;

    return "Unknown";
}
#endif

static void HandleSignal(int sig) {
    try {
        switch (sig) {
            #ifdef Q_OS_LINUX
        case SIGQUIT:
            #endif
        case SIGINT:
        case SIGTERM: {
            qCritical() << "Signal achieved: " << strsignal(sig);
            QCoreApplication::quit();
            break;
        }
        case SIGABRT:
        case SIGSEGV: {
            qCritical() << "Catch signal: " << strsignal(sig);
            #ifdef Q_OS_WIN
            //TODO: need to test this logic on Windows
            constexpr int maxFrames = 64;
            void* stackTrace[maxFrames];
            USHORT frames = CaptureStackBackTrace(0, maxFrames, stackTrace, nullptr);
            SYMBOL_INFO* symbol = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char));
            symbol->MaxNameLen = 255;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

            for (USHORT i = 0; i < frames; ++i) {
                SymFromAddr(GetCurrentProcess(), (DWORD64)(stackTrace[i]), 0, symbol);
                qInfo() << "#" << i << ": " << symbol->Name << " - 0x" << std::hex << stackTrace[i];
            }
            free(symbol);
            #elif defined(Q_OS_LINUX)
            void * stackTrace[64];
            int    frameCount = backtrace(stackTrace, 64);
            char **symbols    = backtrace_symbols(stackTrace, frameCount);
            if (symbols != nullptr) {
                for (int i = 0; i < frameCount; ++i)
                    qInfo() << "#" << i << ": " << symbols[i];
                free(symbols);
            }
            #endif

            if (sig == SIGSEGV)
                _exit(EXIT_FAILURE);
            else
                exit(EXIT_FAILURE);
        }
        default:
            exit(EXIT_FAILURE);
        }
    } catch (const std::exception &ex) {
        qCritical() << "SignalHandler exception: " << ex.what();
        _exit(EXIT_FAILURE);
    }
}

bool SetupSignals() {
    #ifdef Q_OS_WIN
    constexpr const std::array<int, 4> arrSig = { { SIGINT, SIGTERM, SIGSEGV, SIGABRT } };
    #else
    constexpr const std::array<int, 5> arrSig = { { SIGINT, SIGTERM, SIGSEGV, SIGABRT, SIGQUIT } };
    #endif

    for (const auto &item : arrSig) {
        if (signal(item, HandleSignal) == SIG_ERR) {
            qCritical() << "Cannot handle a signal: " + QString::fromStdString(strsignal(item))
                + ", reason - " + QString::fromStdString(strerror(errno));
            return false;
        }
    }

    return true;
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    app.setApplicationName("ExtraChain Console Client");
    app.setOrganizationName("ExtraChain Foundation");
    app.setOrganizationDomain("https://extrachain.io/");
    app.setApplicationVersion(EXTRACHAIN_VERSION);

    if (!SetupSignals())
        exit(EXIT_FAILURE);

    registerMetaTypes();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "ExtraChain (ExC) is a lightweight blockchain infrastructure and decentralized storage ExDFS "
        "allowing creation of high-load dApps (decentralized applications) for both portable, non-portable "
        "and IoT devices.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption debugOption("debug", "Enable debug logs");
    QCommandLineOption clearDataOption("clear-data", "Wipe all data");
    QCommandLineOption dirOption("current-dir", "Set current directory.", "current-dir");
    QCommandLineOption emailOption({ "e", "email" }, "Set email", "email");
    QCommandLineOption passOption({ "s", "password" }, "Set password", "password");
    QCommandLineOption inputOption("disable-input", "Console input disable");
    QCommandLineOption core("core", "First network creation");
    QCommandLineOption importOption("import", "Import from file", "import");
    QCommandLineOption netdebOption("network-debug", "Print all messages. Only for debug build");
    QCommandLineOption dfsLimitOption({ "l", "limit" }, "Set limit", "dfs-limit");
    QCommandLineOption vpnServer("vpn-server", "Start VPN server");
    parser.addOptions(
    { debugOption, dirOption, emailOption, passOption, inputOption, core, clearDataOption,
      importOption, netdebOption, dfsLimitOption, vpnServer });
    parser.process(app);

    // TODO: allow absolute dir
    QString dirName = Utils::fixFileName(parser.value(dirOption), "");
    Utils::dataDir(dirName.isEmpty() ? "console-data" : dirName);
    QDir().mkdir(Utils::dataDir());
    QDir::setCurrent(QDir::currentPath() + QDir::separator() + Utils::dataDir());

    if (parser.isSet(clearDataOption)) {
        Utils::wipeDataFiles();
    }

    //    QLockFile lockFile(".console.lock");
    //    if (!lockFile.tryLock(100)) {
    //        if (!QFile::exists(".console.lock")) {
    //            qDebug() << "[Console] Unable to write files. Check folder permissions";
    //            return -1;
    //        }
    //        qDebug() << "[Console] Already running in directory" << QDir::currentPath();
    //        return -1;
    //    }

    LogsManager::debugLogs = parser.isSet(debugOption);
    #ifdef QT_DEBUG
    LogsManager::debugLogs = !parser.isSet(debugOption);
    Network::networkDebug  = parser.isSet(netdebOption);
    #endif

    qInfo() << " ┌───────────────────────────────────────────┐";
    qInfo().nospace() << " │            ExtraChain " << EXTRACHAIN_VERSION << "." << COMPILE_DATE
        << "         │";
    if (LogsManager::debugLogs)
        qInfo() << " │     Console:" << GIT_COMMIT << "| Core:" << GIT_COMMIT_CORE << "     │";
    qInfo() << " └───────────────────────────────────────────┘";
    LogsManager::etHandler();
    qInfo().noquote().nospace() << "[Build Info] " << Utils::detectCompiler() << ", Qt " << QT_VERSION_STR
        << ", SQLite " << DBConnector::sqlite_version() << ", Sodium "
        << Utils::sodiumVersion().c_str() << ", Boost " << Utils::boostVersion();
    // << ", Boost Asio " << Utils::boostAsioVersion();
    if (QString(GIT_BRANCH) != "dev" || QString(GIT_BRANCH_CORE) != "dev")
        qInfo().noquote() << "[Branches] Console:" << GIT_BRANCH << "| ExtraChain Core:" << GIT_BRANCH_CORE;
    qInfo() << "";
    qDebug() << "[Console] Debug logs on";

    bool           isNewNetwork = parser.isSet(core);
    ConsoleManager console;
    if (!dirName.isEmpty())
        qDebug() << "Custom data directory:" << dirName;

    if (LogsManager::debugLogs)
        LogsManager::print("");

    QString argEmail    = parser.value(emailOption);
    QString argPassword = parser.value(passOption);
    QString email       = argEmail.isEmpty() && !AutologinHash::isAvailable()
                        ? ConsoleManager::getSomething("e-mail")
                        : argEmail;
    QString password = argPassword.isEmpty() && !AutologinHash::isAvailable()
                           ? ConsoleManager::getSomething("password")
                           : argPassword;
    if (argEmail.isEmpty() || argPassword.isEmpty())
        LogsManager::print("");
    if (parser.isSet(inputOption))
        qDebug() << "[Console] Input off";
    else
        console.startInput();

    ExtraChainNode node;
    console.setExtraChainNode(&node);
    console.dfsStart();

    QString dfsLimit = parser.value(dfsLimitOption);
    if (!dfsLimit.isEmpty()) {
        bool    isOk  = false;
        quint64 limit = dfsLimit.toULongLong(&isOk);
        if (isOk) {
            node.dfs()->setBytesLimit(limit);
        }
    }

    if (isNewNetwork)
        node.createNewNetwork(email, password, "Some Coin", "1111", "#ffffff");

    QString importFile = parser.value(importOption);
    if (!importFile.isEmpty()) {
        QFile file(importFile);
        file.open(QFile::ReadOnly);
        auto data = file.readAll().toStdString();
        if (data.empty()) {
            qInfo() << "Incorrect import";
            std::exit(0);
        }
        node.importUser(data, email.toStdString(), password.toStdString());
        file.close();
    }

    if (node.accountController()->count() == 0) {
        std::string   loginHash;
        AutologinHash autologinHash;
        if (AutologinHash::isAvailable() && autologinHash.load()) {
            loginHash = autologinHash.hash();
        } else {
            loginHash = Utils::calcHash((email + password).toStdString());
            password.clear();
        }

        auto result = node.login(loginHash);
        if (!result) {
            if (AccountController::profilesList().size() != 0)
                qInfo() << "Error: Incorrect login or password";
            else
                qInfo() << "Error: No profiles files";
            std::exit(-1);
        }
    } else if (node.accountController()->count() > 0) {
        // if (parser.isSet(vpnServer))
            // node.createVPNKeys();
    }

    return app.exec();
}
