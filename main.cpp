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

//#include "rextrachain/rextrachain.h"

#include "datastorage/dfs/permission_manager.h"

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
                                << Utils::sodiumVersion().c_str() << ", Boost " << Utils::boostVersion()
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

    // dfs temp
    QObject::connect(node->dfs(), &DfsController::added, [](ActorId actorId, std::string fileHash) {
        qInfo() << "[Console/DFS] Added" << actorId << QString::fromStdString(fileHash);
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

    if (node->accountController()->count() == 0)
        emit node->login(email.toUtf8(), password.toUtf8());

    password.clear();

    QObject::connect(node.get(), &ExtraChainNode::ready, [&]() {
        qInfo() << "ready to test permission manager";

        PermissionManager pm(node.get());
        qInfo() << "count accounts: "<< node->accountController()->count();
        qInfo() << "Start permission manager test:";
        const std::string pathToAddFile = "/Users/andreea/Downloads/Ids.pdf";
        const auto actor = node->accountController()->mainActor();
        std::string localFile = node->dfs()->addLocalFile(actor, pathToAddFile, "console",
                                                          DFS::Encryption::Public);

        const auto path = node->dfs()->getFileFromStorage(node->accountController()->mainActor().id(), localFile);
        std::filesystem::path fpath = DFS::Path::convertPathToPlatform(pathToAddFile);
        std::filesystem::path newFilePath = fpath;
        std::string fileHash = Utils::calcKeccakForFile(newFilePath);

        qInfo() << "[Path]: " << QString::fromStdString(path);
        qInfo() << "File hash: " << QString::fromStdString(fileHash);

        DFS::Permission::PermissionMode mode;
        DFS::Permission::AddPermission addPermission;
        addPermission.actor = actor.id().toStdString();
        addPermission.permissionValue = mode.toInt();
        addPermission.path = path;
        addPermission.userId = actor.id().toStdString();
        addPermission.fileHash = fileHash;
        pm.sign(actor, addPermission);

        qInfo() << "sign by signature: " << QString::fromStdString(addPermission.signature) << QString::fromStdString(addPermission.path);
        pm.savePermission(addPermission);

        const auto publicActor = node->actorIndex()->getActor(actor.id());
        qInfo() << "public actor key: "<< publicActor.key();

        qInfo() << "verify: " << pm.verify(publicActor, addPermission);
        Q_ASSERT(pm.verify(publicActor, addPermission) == true);
        {
            qInfo() << "mode is readable: " << pm.isReadable(mode) << " must be - false";;
            Q_ASSERT(pm.isReadable(mode) == false);

            qInfo() << "mode is writable: " << pm.isWritable(mode) << " must be - false";
            Q_ASSERT(pm.isWritable(mode) == false);

            qInfo() << "mode is customizable: " << pm.isCustomizable(mode) << " must be - false";
            Q_ASSERT(pm.isCustomizable(mode) == false);

            qInfo() << "mode is editable: " << pm.isEditable(mode) << " must be - false";
            Q_ASSERT(pm.isEditable(mode) == false);

            qInfo() << "mode has permission: " << pm.hasPermission(mode) << " must be - false";
            Q_ASSERT(pm.hasPermission(mode) == false);

            qInfo() << "mode has permission: " << pm.noPermission(mode) << " must be - true";
            Q_ASSERT(pm.noPermission(mode) == true);
        }

        pm.addPermission(mode, DFS::Permission::Read);
        {
            qInfo() << "mode is readable: " << pm.isReadable(mode) << " must be - true";
            Q_ASSERT(pm.isReadable(mode) == true);

            qInfo() << "mode is writable: " << pm.isWritable(mode) << " must be - false";
            Q_ASSERT(pm.isWritable(mode) == false);

            qInfo() << "mode is customizable: " << pm.isCustomizable(mode) << " must be - false";
            Q_ASSERT(pm.isCustomizable(mode) == false);

            qInfo() << "mode is editable: " << pm.isEditable(mode) << " must be - false";
            Q_ASSERT(pm.isEditable(mode) == false);

            qInfo() << "mode has permission: " << pm.hasPermission(mode) << " must be - true";
            Q_ASSERT(pm.hasPermission(mode) == true);

            qInfo() << "mode has permission: " << pm.noPermission(mode) << " must be - false";
            Q_ASSERT(pm.noPermission(mode) == false);
        }

        pm.addPermission(mode, DFS::Permission::Write);
        {
            qInfo() << "mode is readable: " << pm.isReadable(mode) << " must be - true";
            Q_ASSERT(pm.isReadable(mode) == true);

            qInfo() << "mode is writable: " << pm.isWritable(mode) << " must be - true";
            Q_ASSERT(pm.isWritable(mode) == true);

            qInfo() << "mode is customizable: " << pm.isCustomizable(mode) << " must be - false";
            Q_ASSERT(pm.isCustomizable(mode) == false);

            qInfo() << "mode is editable: " << pm.isEditable(mode) << " must be - false";
            Q_ASSERT(pm.isEditable(mode) == false);

            qInfo() << "mode has permission: " << pm.hasPermission(mode) << " must be - true";
            Q_ASSERT(pm.hasPermission(mode) == true);

            qInfo() << "mode has permission: " << pm.noPermission(mode) << " must be - false";
            Q_ASSERT(pm.noPermission(mode) == false);
        }

        pm.addPermission(mode, DFS::Permission::Edit);
        {
            qInfo() << "mode is readable: " << pm.isReadable(mode) << " must be - true";
            Q_ASSERT(pm.isReadable(mode) == true);

            qInfo() << "mode is writable: " << pm.isWritable(mode) << " must be - true";
            Q_ASSERT(pm.isWritable(mode) == true);

            qInfo() << "mode is customizable: " << pm.isCustomizable(mode) << " must be - false";
            Q_ASSERT(pm.isCustomizable(mode) == false);

            qInfo() << "mode is editable: " << pm.isEditable(mode) << " must be - true";
            Q_ASSERT(pm.isEditable(mode) == true);

            qInfo() << "mode has permission: " << pm.hasPermission(mode) << " must be - true";
            Q_ASSERT(pm.hasPermission(mode) == true);

            qInfo() << "mode has permission: " << pm.noPermission(mode) << " must be - false";
            Q_ASSERT(pm.noPermission(mode) == false);
        }

        pm.addPermission(mode, DFS::Permission::Custom);
        {
            qInfo() << "mode is readable: " << pm.isReadable(mode) << " must be - true";
            Q_ASSERT(pm.isReadable(mode) == true);

            qInfo() << "mode is writable: " << pm.isWritable(mode) << " must be - true";
            Q_ASSERT(pm.isWritable(mode) == true);

            qInfo() << "mode is customizable: " << pm.isCustomizable(mode) << " must be - true";
            qInfo() << "mode is editable: " << pm.isEditable(mode) << " must be - true";
            qInfo() << "mode has permission: " << pm.hasPermission(mode) << " must be - true";
            qInfo() << "mode has permission: " << pm.noPermission(mode) << " must be - false";

            pm.addPermission(mode, DFS::Permission::Remove);
            qInfo() << "mode is readable: " << pm.isReadable(mode) << " must be - true";
            Q_ASSERT(pm.isReadable(mode) == true);

            qInfo() << "mode is writable: " << pm.isWritable(mode) << " must be - true";
            Q_ASSERT(pm.isWritable(mode) == true);

            qInfo() << "mode is customizable: " << pm.isCustomizable(mode) << " must be - true";
            Q_ASSERT(pm.isCustomizable(mode) == true);

            qInfo() << "mode is editable: " << pm.isEditable(mode) << " must be - true";
            Q_ASSERT(pm.isEditable(mode) == true);

            qInfo() << "mode is removable: " << pm.isRemovable(mode) << " must be - true";
            Q_ASSERT(pm.isRemovable(mode) == true);

            qInfo() << "mode has permission: " << pm.hasPermission(mode) << " must be - true";
            Q_ASSERT(pm.hasPermission(mode) == true);

            qInfo() << "mode has permission: " << pm.noPermission(mode) << " must be - false";
            Q_ASSERT(pm.noPermission(mode) == false);
        }

        pm.addPermission(mode, DFS::Permission::Service);
        {
            qInfo() << "mode is readable: " << pm.isReadable(mode) << " must be - true";
            Q_ASSERT(pm.isReadable(mode) == true);

            qInfo() << "mode is writable: " << pm.isWritable(mode) << " must be - true";
            Q_ASSERT(pm.isWritable(mode) == true);

            qInfo() << "mode is customizable: " << pm.isCustomizable(mode) << " must be - true";
            Q_ASSERT(pm.isCustomizable(mode) == true);

            qInfo() << "mode is editable: " << pm.isEditable(mode) << " must be - true";
            Q_ASSERT(pm.isEditable(mode) == true);

            qInfo() << "mode is removable: " << pm.isRemovable(mode) << " must be - true";
            Q_ASSERT(pm.isRemovable(mode) == true);

            qInfo() << "mode is serviceable: " << pm.isServiceable(mode) << " must be - true";
            Q_ASSERT(pm.isServiceable(mode) == true);

            qInfo() << "mode has permission: " << pm.hasPermission(mode) << " must be - true";
            Q_ASSERT(pm.hasPermission(mode) == true);

            qInfo() << "mode has permission: " << pm.noPermission(mode) << " must be - false";
            Q_ASSERT(pm.noPermission(mode) == false);

        }

        //test update
        const int savedPreviousPV = mode.toInt();
        pm.removePermission(mode, DFS::Permission::Read);
        Q_ASSERT(savedPreviousPV != mode.toInt());

        addPermission.permissionValue = mode.toInt();
        pm.sign(actor, addPermission);
        pm.updatePermission(addPermission);

        //test search file permission by hash
        auto list = pm.searchFileByHash(path, fileHash);
        qInfo() << "count founded files:" << list.size() << ". Must be not 0.";
        Q_ASSERT(list.size() != 0);
        Q_ASSERT(list.size() == 1);

        const std::string nonExisthashFile = "3e55f3da4011200f000";
        list = pm.searchFileByHash(path, nonExisthashFile);
        qInfo() << "count founded files:" << list.size() << ". Must be 0.";
        Q_ASSERT(list.size() == 0);
        Q_ASSERT(list.size() != 1);

        //test search file prmittion by name
        list = pm.searchFileById(path, actor.id().toStdString());
        qInfo() << "count founded files:" << list.size() << ". Must be not 0.";
        Q_ASSERT(list.size() != 0);
        Q_ASSERT(list.size() == 1);

        std::string nonExistPartNameFile = "888843fce4";
        list = pm.searchFileById(path, nonExistPartNameFile);
        qInfo() << "count founded files:" << list.size() << ". Must be 0.";
        Q_ASSERT(list.size() == 0);
        Q_ASSERT(list.size() != 1);

        qInfo() << "Finished test permission manager";
    });

    return app.exec();
}
