#include "console/console_manager.h"

#include <QProcess>
#include <QTextStream>

#include "datastorage/dfs/dfs_controller.h"
#include "datastorage/index/actorindex.h"
#include "managers/extrachain_node.h"
#include "managers/logs_manager.h"
#include "managers/thread_pool.h"
#include "network/isocket_service.h"

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
    command = command.simplified().toLower();
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
        auto mainActorId = node->accountController()->mainActor().id();
        ActorId firstId = node->actorIndex()->firstId();

        QStringList sendtx = command.split(" ");
        if (sendtx.length() == 3) {
            QByteArray toId = sendtx[1].toUtf8();
            QByteArray toAmount = sendtx[2].toUtf8();
            qDebug() << "sendtx" << toId << toAmount;

            ActorId receiver(toId.toStdString());
            BigNumber amount = Transaction::visibleToAmount(toAmount);

            if (mainActorId != firstId)
                node->createTransaction(receiver, amount, ActorId());
            else
                node->createTransactionFrom(firstId, receiver, amount, ActorId());
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
                auto mainId = node->accountController()->mainActor().id();
                qInfo() << "User" << mainId;
                for (const auto &actor : actors) {
                    if (actor.id() != node->accountController()->mainActor().id()) {
                        qInfo() << "Wallet" << actor.id();
                    }
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
        qInfo() << "Adding file to DFS:" << command.mid(8).data();

        node->dfs()->addLocalFile(node->accountController()->mainActor(), file, "console",
                                  DFS::Encryption::Public);
    }

    if (command.left(8) == "dfs get ") {
        auto list = command.split(" ");
        if (list.size() < 4) {
            qInfo() << "List has less 2 parameters";
        } else {
            const std::string pathToNewFolder = list[2].toStdString();
            const std::string pathToDfsFile = list[3].toStdString();
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
            QString("%1.extrachain").arg(node->accountController()->mainActor().id().toString());
        QFile file(fileName);
        file.open(QFile::WriteOnly);
        if (file.write(data.toUtf8()) > 1)
            qInfo() << "Exported to" << fileName;
        file.close();
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

void ConsoleManager::dfsStat() {
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
