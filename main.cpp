#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QLockFile>
#include <QStandardPaths>

#include "console/console_manager.h"
#include "managers/extrachain_node.h"
#include "managers/logs_manager.h"
//#include "dfs/packages/headers/dfs_universal.h"
#include "dfs/packages/headers/dfs_request.h"
//#include "network/packages/service/downloaddfsrequest.h"
#include "enc/enc_tools.h"
#include "metatypes.h"

#include "rextrachain/rextrachain.h"

#ifndef EXTRACHAIN_CMAKE
    #include "preconfig.h"
#endif

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

#ifdef EXTRACHAIN_ENABLE_RUST
    qDebug() << "[Rust]" << EXTRACHAIN_RUST_VERSION << "|" << number_42();
#endif

    app.setApplicationName("ExtraChain Console Client");
    app.setOrganizationName("ExtraChain Foundation");
    app.setOrganizationDomain("https://extrachain.io/");
    app.setApplicationVersion(EXTRACHAIN_VERSION);

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
    QCommandLineOption emailOption({ "e", "email" }, "Set email.", "email");
    QCommandLineOption passOption({ "s", "password" }, "Set password.", "password");
    QCommandLineOption inputOption("disable-input", "Console input disable");
    QCommandLineOption createNetworkOption("create-network", "First network creation");
    parser.addOptions({ debugOption, dirOption, emailOption, passOption, inputOption, createNetworkOption,
                        clearDataOption });
    parser.process(app);

    // TODO: allow absolute dir
    QString dirName = Utils::fixFileName(parser.value(dirOption), "");
    Utils::dataDir(dirName.isEmpty() ? "console-data" : dirName);
    QDir().mkdir(Utils::dataDir());
    QDir::setCurrent(QDir::currentPath() + QDir::separator() + Utils::dataDir());

    if (parser.isSet(clearDataOption)) {
        Utils::wipeDataFiles();
    }

    QLockFile lockFile(".console.lock");
    if (!lockFile.tryLock(100)) {
        qDebug() << "[Console] Already running in directory" << QDir::currentPath();
        return -1;
    }

    LogsManager::debugLogs = parser.isSet(debugOption);
#ifdef QT_DEBUG
    LogsManager::debugLogs = !parser.isSet(debugOption);
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
                                << SecretKey::sodium_version().c_str() << ", Boost " << Utils::boostVersion()
                                << ", Boost Asio " << Utils::boostAsioVersion();
    if (QString(GIT_BRANCH) != "dev" || QString(GIT_BRANCH_CORE) != "dev")
        qInfo().noquote() << "[Branches] Console:" << GIT_BRANCH << "| ExtraChain Core:" << GIT_BRANCH_CORE;
    qInfo() << "";
    qDebug() << "[Console] Debug logs on";

    bool isNewNetwork = parser.isSet(createNetworkOption);
    ConsoleManager console;
    if (!dirName.isEmpty())
        qDebug() << "Custom data directory:" << dirName;

    if (LogsManager::debugLogs)
        LogsManager::print("");

    QString argEmail = parser.value(emailOption);
    QString argPassword = parser.value(passOption);
    QString email = argEmail.isEmpty() ? ConsoleManager::getSomething("e-mail") : argEmail;
    QString password = argPassword.isEmpty() ? ConsoleManager::getSomething("password") : argPassword;
    if (argEmail.isEmpty() || argPassword.isEmpty())
        LogsManager::print("");
    if (parser.isSet(inputOption))
        qDebug() << "[Console] Input off";
    else
        console.startInput();

    auto node = std::make_shared<ExtraChainNode>();
    console.setExtraChainNode(node);

    if (isNewNetwork)
        node->createNewNetwork(email, password, "Etalonium Coin", "1111", "#fa4868");

    if (node->accountController()->getAccountCount() == 0)
        emit node->login(email.toUtf8(), password.toUtf8());

    password.clear();

    return app.exec();
}
