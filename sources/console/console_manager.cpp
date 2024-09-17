#include "console/console_manager.h"

#include <QProcess>
#include <QTextStream>

#include "datastorage/dfs/dfs_controller.h"
#include "datastorage/index/actorindex.h"
#include "managers/extrachain_node.h"
#include "managers/logs_manager.h"
#include "managers/thread_pool.h"
#include "network/isocket_service.h"
#include "datastorage/blockchain.h"
#include "managers/tx_manager.h"

#ifdef Q_OS_UNIX
    #include <unistd.h> // STDIN_FILENO
#endif
#ifdef Q_OS_WIN
    #include "Windows.h"
#endif

ConsoleManager::ConsoleManager(QObject *parent)
    : QObject(parent)
#ifdef Q_OS_UNIX
    , notifier(STDIN_FILENO, QSocketNotifier::Read)
    , notifierInput(stdin, QIODevice::ReadOnly)
#endif
{
    m_pushManager = new PushManager(node);
}

ConsoleManager::~ConsoleManager() {
    qDebug() << "[Console] Stop";
}

void ConsoleManager::commandReceiver(QString command) {
    command = command.simplified();
    qDebug() << "[Console] Input:" << command;

    // TODO: process coin request
    //    if (node->listenCoinRequest())
    //    {
    //        auto &requestQueue = node->requestCoinQueue();
    //        auto request = requestQueue.takeFirst();

    //        if (command == "y")
    //        {
    //            auto [receiver, amount, plsr] = request;
    //            node->sendCoinRequest(receiver, amount);
    //        }

    //        node->setListenCoinRequest(false);
    //        if (requestQueue.length() > 0)
    //        {
    //            request = requestQueue.takeFirst();
    //            auto [receiver, amount, plsr] = request;
    //            node->coinResponse(receiver, amount, plsr);
    //        }

    //        return;
    //    }

    if (command == "quit" || command == "exit") {
        qInfo() << "Exit...";
        qApp->quit();
    }

    if (command == "wipe") {
        Utils::wipeDataFiles();
        qInfo() << "Wiped and exit...";
        qApp->quit();
    }

    if (command == "logs on") {
        LogsManager::on();
        qInfo() << "Logs enabled";
    }

    if (command == "logs off") {
        LogsManager::off();
        qInfo() << "Logs disabled";
    }

    if (command.left(3) == "dir") {
        auto list = command.split(" ");
        if (list.length() == 2) {
            QString program = list[1].simplified();

            if (!program.isEmpty()) {
                QProcess::execute(program, { QDir::currentPath() });
                return;
            }
        }

#if defined(Q_OS_LINUX)
        QProcess::execute("xdg-open", { QDir::currentPath() });
#elif defined(Q_OS_WIN)
        QProcess::execute("explorer.exe", { QDir::currentPath().replace("/", "\\") });
#elif defined(Q_OS_MAC)
        QProcess::execute("open", { QDir::currentPath() });
#else
        qInfo() << "Command \"dir\" not implemented for this platform";
#endif
    }

    if (command.left(6) == "sendtx") {
        qDebug() << "sendtx";
        auto mainActorId = node->accountController()->mainActor()->id();
        ActorId firstId = node->actorIndex()->firstId();

        QStringList sendtx = command.split(" ");
        if (sendtx.length() == 3) {
            QByteArray toId = sendtx[1].toUtf8();
            QByteArray toAmount = sendtx[2].toUtf8();
            qDebug() << "sendtx" << toId << toAmount;

            ActorId receiver(toId.toStdString());

            BigNumberFloat amount = Transaction::visibleToAmount(toAmount.toStdString());

            // BigNumberFloat amount(toAmount.toStdString());
            if (toId == "burn") {
                receiver = ActorId();
            }

            Transaction tx(mainActorId, receiver, amount);
            node->txManager()->addTransaction(tx);
            node->network()->send_message(tx, MessageType::BlockchainTransaction);

//            if (mainActorId != firstId)
//            node->createTransaction(receiver, BigNumberFloat(10), ActorId());
//            else
//                node->createTransactionFrom(firstId, receiver, BigNumberFloat(10), ActorId());
        }
    }

    if (command == "cn count" || command == "connections count") {
        qInfo() << "Connections:" << node->network()->connections().length();
    }

    if (command == "cn list" || command == "connections list") {
        auto &connections = node->network()->connections();

        if (connections.length() > 0) {
            qInfo() << "Connections:";
            std::for_each(connections.begin(), connections.end(), [](auto &el) {
                qInfo().noquote() << el->ip() << el->port() << el->serverPort() << el->isActive()
                                  << el->protocolString() << el->identifier();
            });
        } else {
            qInfo() << "No connections";
        }
        qInfo() << "-----------";
    }

    if (command.left(7) == "connect") {
        auto list = command.split(" ");
        if (list.length() != 3)
            return;

        QString ip = list[2];
        if (ip == "local")
            ip = Utils::findLocalIp().ip().toString();
        QString protocol = list[1];

        if (Utils::isValidIp(ip) && (protocol == "udp" || protocol == "ws")) {
            auto networkProtocol = Network::Protocol::WebSocket;
            qInfo().noquote() << "Connect to" << ip << protocol;
            node->network()->connectToNode(ip, networkProtocol);
        } else {
            qInfo() << "Invalid connect input";
        }
    }

    if (command.left(7) == "wallet ") {
        auto list = command.split(" ");
        if (list.length() > 1) {
            if (list[1] == "new") {
                auto actor = node->accountController()->createWallet();
                qInfo() << "Wallet created:" << actor.id();
            }

            if (list[1] == "list") {
                qInfo() << "Wallets:";
                auto actors = node->accountController()->accounts();
                auto mainId = node->accountController()->mainActor()->id();
                qInfo() << "User" << mainId;
                for (const auto &actor : actors) {
                    if (actor->id() != node->accountController()->mainActor()->id()) {
                        qInfo() << "Wallet" << actor->id();
                    }
                }
            }

            if (list[1] == "balance" || list[1] == "check_balance") {
                const auto actors = node->actorIndex()->allActorsStd();
                for (int i = 0; i < actors.size(); i++) {
                    const auto balance = node->blockchain()->getUserBalance(ActorId(actors[i]), ActorId());
                    std::cout << "[Actor: " << actors[i].c_str() << "] | ["
                              << "Balance: " << balance << "]" << std::endl;
                }
            }
        }
    }

    if (command.left(4) == "push") {
        command.replace(QRegularExpression("\\s+"), " ");
        QString actorId = command.mid(5, 20);
        Notification notify { .time = 100, .type = Notification::NewPost, .data = actorId.toLatin1() + " " };
        m_pushManager->pushNotification(actorId, notify);
    }

    if (command.left(8) == "dfs add ") {
        auto file = command.mid(8).toStdWString();
        std::filesystem::path filepath(file);
        qInfo() << "Adding file to DFS:" << command.mid(8).data();

        auto result = node->dfs()->addLocalFile(node->accountController()->mainActor(), file,
                                                filepath.filename().string(), DFS::Encryption::Public);
        auto res = QString::fromStdString(result);
        if (res.left(5) == "Error") {
            qDebug() << res;
        }
    }

    if (command.left(8) == "dfs get ") {
        auto list = command.split(" ");
        if (list.size() < 4) {
            qInfo() << "List has less 2 parameters";
        } else {
            const std::string pathToNewFolder = list[2].toStdString();
            const std::string pathToDfsFile = list[3].toStdString();
            if (pathToNewFolder.empty() || pathToDfsFile.empty()) {
                qDebug() << "One or more parameters is empty. Please check in parameters.";
                return;
            }

            if (list.size() == 5) {
                node->dfs()->exportFile(pathToNewFolder, pathToDfsFile, list[4].toStdString());
            } else if (list.size() == 4) {
                node->dfs()->exportFile(pathToNewFolder, pathToDfsFile);
            }
        }
    }

    if (command.left(6) == "export") {
        auto data = QString::fromStdString(node->exportUser());
        QString fileName =
            QString("%1.extrachain").arg(node->accountController()->mainActor()->id().toString());
        QFile file(fileName);
        file.open(QFile::WriteOnly);
        if (file.write(data.toUtf8()) > 1)
            qInfo() << "Exported to" << fileName;
        file.close();
    }

    if (command.left(16) == "list_user_files ") {
        auto userId = command.split(" ")[1];
        qInfo() << "show list user " << userId << " files";
        std::filesystem::path actorFolderPath = DFSB::fsActrRoot + "/" + userId.toStdString();
        std::cout << "======================================================" << std::endl;

        for (const auto &entry : std::filesystem::recursive_directory_iterator(actorFolderPath)) {
            const auto fileName = entry.path().filename();
            if (fileName == "." || fileName == ".." || fileName == ".dir" || fileName == ".DS_Store")
                continue;

            if (!std::filesystem::is_directory(entry)) {
                const std::string filePath =
                    actorFolderPath.string() + "/" + entry.path().filename().string();
                std::cout << entry.path() << std::endl;
            } else {
                std::cout << "------------------------------------------------------" << std::endl;
            }
        }
        std::cout << "======================================================" << std::endl;
    }
    // request_coins coins
    if (command.left(13) == "request_coins") {
        auto actorId = node->accountController()->mainActor()->id();
        auto coins = command.split(" ")[1];

        qInfo() << "Request coins: " << coins << "for " << actorId.toString();
        // node->blockchain()->sendCoinReward(actorId, coins.toInt());
    }
}

PushManager *ConsoleManager::pushManager() const {
    return m_pushManager;
}

void ConsoleManager::setExtraChainNode(ExtraChainNode *value) {
    node = value;

    // auto dfs = node->dfs();
    connect(node, &ExtraChainNode::pushNotification, m_pushManager, &PushManager::pushNotification);
    // connect(dfs, &Dfs::chatMessage, m_pushManager, &PushManager::chatMessage);
    // connect(dfs, &Dfs::fileAdded, m_pushManager, &PushManager::fileAdded);
    // connect(resolver, &ResolveManager::saveNotificationToken, this,
    // &ConsoleManager::saveNotificationToken);
}

void ConsoleManager::startInput() {
#if defined(Q_OS_WIN)
    if (IsDebuggerPresent()) {
        qDebug() << "[Console] Input off, because debugger mode";
        return;
    }

    #ifdef Q_OS_WINDOWS
    DWORD consoleMode;
    bool isInteractive = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &consoleMode);
    if (!isInteractive) {
        qDebug() << "[Console] Console is not interactive, command input is disabled";
        return;
    }
    #endif

    connect(&consoleInput, &ConsoleInput::input, this, &ConsoleManager::commandReceiver);
    ThreadPool::addThread(&consoleInput);
#elif defined(Q_OS_UNIX)
    connect(&notifier, &QSocketNotifier::activated, [this] {
        QString line = notifierInput.readLine();
        if (!line.isEmpty())
            commandReceiver(line);
    });
#endif
}

void ConsoleManager::saveNotificationToken(QByteArray os, ActorId actorId, ActorId token) {
    m_pushManager->saveNotificationToken(os, actorId, token);
}

void ConsoleManager::dfsStart() {
    QObject::connect(node->dfs(), &DfsController::added,
                     [](ActorId actorId, std::string fileHash, std::string visual, uint64_t size) {
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
}

QString ConsoleManager::getSomething(const QString &name) {
    QString something;

    qInfo().noquote().nospace() << "Enter " + name + ":";

    QTextStream cin(stdin);
    while (something.isEmpty()) {
        something = cin.readLine();

        if (something.indexOf(" ") != -1) {
            qInfo() << "Please enter without spaces";
            something = "";
            continue;
        }
    }

    if (something == "wipe") {
        Utils::wipeDataFiles();
        qInfo() << "Wiped and exit...";
        std::exit(0);
    }

    if (something == "empty")
        something = "";

    return something;
}
