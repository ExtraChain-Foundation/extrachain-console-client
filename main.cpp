#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QLockFile>
#include <QStandardPaths>

#include "console/console_manager.h"
#include "datastorage/dfs/dfs_controller.h"
#include "managers/extrachain_node.h"
#include "managers/logs_manager.h"
#include "metatypes.h"

#ifndef EXTRACHAIN_CMAKE
    #include "preconfig.h"
#endif

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

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
    QCommandLineOption emailOption({ "e", "email" }, "Set email", "email");
    QCommandLineOption passOption({ "s", "password" }, "Set password", "password");
    QCommandLineOption inputOption("disable-input", "Console input disable");
    QCommandLineOption createNetworkOption("create-network", "First network creation");
    QCommandLineOption importOption("import", "Import from file", "import");
    parser.addOptions({ debugOption, dirOption, emailOption, passOption, inputOption, createNetworkOption,
                        clearDataOption, importOption });
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
        if (!QFile::exists(".console.lock")) {
            qDebug() << "[Console] Unable to write files. Check folder permissions";
            return -1;
        }
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
                                << Utils::sodiumVersion().c_str() << ", Boost " << Utils::boostVersion();
    // << ", Boost Asio " << Utils::boostAsioVersion();
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

    // dfs temp
    QObject::connect(node->dfs(), &DfsController::added,
                     [](ActorId actorId, std::string fileHash, std::string visual, int64_t size) {
                         qInfo() << "[Console/DFS] Added" << actorId << QString::fromStdString(fileHash)
                                 << QString::fromStdString(visual) << size;
                     });
    QObject::connect(node->dfs(), &DfsController::uploaded, [](ActorId actorId, std::string fileHash) {
        qInfo() << "[Console/DFS] Uploaded" << actorId << QString::fromStdString(fileHash);
    });
    QObject::connect(node->dfs(), &DfsController::downloaded, [](ActorId actorId, std::string fileHash) {
        qInfo() << "[Console/DFS] Downloaded" << actorId << QString::fromStdString(fileHash);
    });
    QObject::connect(node->dfs(), &DfsController::downloadProgress,
                     [](ActorId actorId, std::string hash, int progress) {
                         qInfo() << "[Console/DFS] Download progress:" << actorId
                                 << QString::fromStdString(hash) << progress;
                     });
    QObject::connect(node->dfs(), &DfsController::uploadProgress,
                     [](ActorId actorId, std::string fileHash, int progress) {
                         qInfo() << "[Console/DFS] Upload progress:" << actorId
                                 << QString::fromStdString(fileHash) << " " << progress;
                     });

    if (isNewNetwork)
        node->createNewNetwork(email, password, "Etalonium Coin", "1111", "#fa4868");

    QString importFile = parser.value(importOption);
    if (!importFile.isEmpty()) {
        QFile file(importFile);
        file.open(QFile::ReadOnly);
        auto data = file.readAll().toStdString();
        if (data.empty()) {
            qInfo() << "Incorrect import";
            std::exit(0);
        }
        node->importUser(data, email.toStdString(), password.toStdString());
        file.close();
    }

    if (node->accountController()->count() == 0)
        emit node->login(email.toUtf8(), password.toUtf8());

    password.clear();

    return app.exec();
}
