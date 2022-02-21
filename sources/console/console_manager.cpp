#include "console/console_manager.h"
#include "managers/extrachain_node.h"
#include "managers/logs_manager.h"
#include "managers/thread_pool.h"
#include "network/isocket_service.h"
#include "profile/private_profile.h"
#include "resolve/resolve_manager.h"

#include <QProcess>
#include <QTextStream>

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
    m_pushManager = new PushManager(this);
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
        auto mainActorId = accController->mainActor().id();
        ActorId firstId = node->actorIndex()->firstId();

        QStringList sendtx = command.split(" ");
        if (sendtx.length() == 3) {
            QByteArray toId = sendtx[1].toUtf8();
            QByteArray toAmount = sendtx[2].toUtf8();
            qDebug() << "sendtx" << toId << toAmount;

            ActorId receiver(toId);
            BigNumber amount = Transaction::visibleToAmount(toAmount);

            if (mainActorId != firstId)
                node->createTransaction(receiver, amount, ActorId());
            else
                node->createTransactionFrom(firstId, receiver, amount, ActorId());
        }
    }

    if (command == "cn count" || command == "connections count") {
        qInfo() << "Connections:" << node->networkManager()->connections().length();
        qInfo() << "DFS Connections:" << node->dfs()->networkManager()->connections().length();
    }

    if (command == "cn list" || command == "connections list") {
        auto &connections = node->networkManager()->connections();
        auto &dfsConnections = node->dfs()->networkManager()->connections();
        auto print = [](auto el) {
            qInfo().noquote() << el->ip() << el->port() << el->serverPort() << el->isActive()
                              << el->protocolString() << el->identifier();
        };

        if (connections.length() > 0) {
            qInfo() << "Connections:";
            std::for_each(connections.begin(), connections.end(), print);
        }
        if (dfsConnections.length() > 0) {
            qInfo() << "DFS Connections:";
            std::for_each(dfsConnections.begin(), dfsConnections.end(), print);
        }
        if (connections.length() == 0 && dfsConnections.length() == 0)
            qInfo() << "No connections";
        qInfo() << "-----------";
    }

    // TODO: actor create LOGIN PASSWORD

    if (command.left(14) == "network create") {
        // TODO
        return;
        QString loginPassword = command.mid(15);

        auto list = loginPassword.indexOf(" ") != -1 ? loginPassword.split(" ") : QStringList {};

        if (loginPassword.isEmpty() || list.length() != 2) {
            qInfo() << "Incorrect login or password.";
            qInfo() << "Command usage: network create LOGIN PASSWORD";
            return;
        }

        // node->createNewNetwork(list[0], list[1]);
    }

    if (command.left(7) == "connect") {
        auto list = command.split(" ");
        if (list.length() != 3)
            return;

        QString ip = list[2];
        if (ip == "local")
            ip = Utils::findLocalIp().ip().toString();
        QString protocol = list[1];

        if (Utils::isValidIp(ip) && (protocol == "tcp" || protocol == "ws")) {
            auto networkProtocol = protocol == "tcp" ? Network::Protocol::Tcp : Network::Protocol::WebSocket;
            qInfo().noquote() << "Connect to" << ip << protocol;
            node.get()->networkManager()->connectToNode(ip, networkProtocol);
            emit node.get()->dfs()->connectToNode(ip, networkProtocol);
        } else {
            qInfo() << "Invalid connect input";
        }
    }

    if (command.left(10) == "disconnect") {
        // qInfo().noquote() << "Disconnect";
        // emit node.get()->removeConnection("");
    }

    if (command.left(4) == "gena") {
        auto list = command.split(" ");

        if (list.length() != 2)
            return;

        long long count = list[1].toLongLong();

        // generate as wallets for now
        for (long long i = 0; i != count; ++i) {
            auto hash = node->privateProfile()->hash();
            auto actor = accController->createActor(ActorType::User, hash);
            emit node->savePrivateProfile(hash, actor.id());
        }
        qInfo() << "----------------------------------------------------------------";
        qInfo() << "Actors generation complete, count:" << count;
    }

    if (command.left(4) == "push") {
        command.replace(QRegularExpression("\\s+"), " ");
        QString actorId = command.mid(5, 20);
        Notification notify { .time = 100, .type = Notification::NewPost, .data = actorId.toLatin1() + " " };
        m_pushManager->pushNotification(actorId, notify);
    }
}

PushManager *ConsoleManager::pushManager() const {
    return m_pushManager;
}

void ConsoleManager::setExtraChainNode(const std::shared_ptr<ExtraChainNode> &value) {
    node = value;
    accController = node->accountController();
    networkManager = node->networkManager();

    m_pushManager->setAccController(accController);
    auto dfs = node->dfs();
    auto resolver = node->resolveManager();
    connect(node.get(), &ExtraChainNode::pushNotification, m_pushManager, &PushManager::pushNotification);
    connect(dfs, &Dfs::chatMessage, m_pushManager, &PushManager::chatMessage);
    connect(dfs, &Dfs::fileAdded, m_pushManager, &PushManager::fileAdded);
    connect(resolver, &ResolveManager::saveNotificationToken, this, &ConsoleManager::saveNotificationToken);

    connect(node->privateProfile(), &PrivateProfile::loginError, [](int error) {
        switch (error) {
        case 1:
            qInfo() << "! Profile files not found";
            break;
        case 2:
            qInfo() << "! Incorrect email or password";
            break;
        }

        if (error != 0)
            qApp->exit();
    });
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
