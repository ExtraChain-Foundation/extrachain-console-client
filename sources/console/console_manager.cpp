/*
 * ExtraChain Console Client
 * Copyright (C) 2025 ExtraChain Foundation <official@extrachain.io>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "console/console_manager.h"

#include <QProcess>
#include <QTextStream>

#include "dfs/dfs_controller.h"
#include "blockchain/actor_index.h"
#include "managers/extrachain_node.h"
#include "managers/logs_manager.h"
#include "managers/thread_pool.h"
#include "network/network_manager.h"
#include "network/isocket_service.h"
#include "blockchain/blockchain.h"
#include "managers/transaction_manager.h"

#ifdef Q_OS_UNIX
    #include <unistd.h> // STDIN_FILENO
#endif
#ifdef Q_OS_WIN
    #include "Windows.h"
#endif

ConsoleManager::ConsoleManager(QObject *parent)
    : QObject(parent)
#ifdef Q_OS_UNIX
// , notifier(STDIN_FILENO, QSocketNotifier::Read)
// , notifierInput(stdin, QIODevice::ReadOnly)
#endif
{
    m_pushManager = new PushManager(node);
}

ConsoleManager::~ConsoleManager() {
    eLog("[Console] Stop");
}

void ConsoleManager::commandReceiver(QString command) {
    command = command.simplified();
    eLog("[Console] Input: {}", command);

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
        eInfo("Exit...");
        qApp->quit();
    }

    if (command == "wipe") {
        Utils::wipeDataFiles();
        eInfo("Wiped and exit...");
        qApp->quit();
    }

    if (command == "logs on") {
        LogsManager::on();
        eInfo("Logs enabled");
    }

    if (command == "logs off") {
        LogsManager::off();
        eInfo("Logs disabled");
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
        eInfo("Command \"dir\" not implemented for this platform");
#endif
    }

    if (command.left(6) == "transaction") {
        eLog("[Console] 'transaction' command");
        auto    mainActorId = node->accountController()->mainActor().id();
        ActorId firstId     = node->actorIndex()->firstId();

        QStringList sendtx = command.split(" ");
        if (sendtx.length() == 3) {
            QByteArray     toId   = sendtx[1].toUtf8();
            BigNumberFloat amount = BigNumberFloat(sendtx[2].toStdString());
            eLog("transaction {} {}", toId, amount.to_string(NumeralBase::Dec));

            ActorId receiver(toId.toStdString());

            // BigNumberFloat amount(toAmount.toStdString());
            if (toId == "burn") {
                receiver = ActorId();
            }

            Transaction tx(mainActorId, receiver, amount);
            // createTransaction
            node->sendTransaction(tx, node->accountController()->mainActor());

            //            if (mainActorId != firstId)
            //            node->createTransaction(receiver, BigNumberFloat(10), ActorId());
            //            else
            //                node->createTransactionFrom(firstId, receiver, BigNumberFloat(10), ActorId());
        }
    }

    if (command == "cn count" || command == "connections count") {
        eInfo("Connections: {}", node->network()->connections()->size());
    }

    if (command == "cn list" || command == "connections list") {
        auto connections = *node->network()->connections();

        if (connections->size() > 0) {
            eInfo("Connections:");
            std::for_each(connections->begin(), connections->end(), [](auto &el) {
                qInfo().noquote() << el->ip() << el->port() << el->server_port() << el->is_active()
                                  << el->protocol_string() << el->identifier();
            });
        } else {
            eInfo("No connections");
        }
        eInfo("-----------");
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
            eInfo("Invalid connect input");
        }
    }

    if (command.left(7) == "wallet ") {
        auto list = command.split(" ");
        if (list.length() > 1) {
            if (list[1] == "new") {
                auto actor = node->accountController()->createWallet();
                eInfo("Wallet created: {}", actor.id());
            }

            if (list[1] == "list") {
                eInfo("Wallets:");
                auto actors = node->accountController()->accounts();
                auto mainId = node->accountController()->mainActor().id();
                eInfo("User {}", mainId);
                for (const auto &actor : actors) {
                    if (actor.id() != node->accountController()->mainActor().id()) {
                        eInfo("Wallet {}", actor.id());
                    }
                }
            }

            if (list[1] == "balance" || list[1] == "check_balance") {
                const auto actors = node->actorIndex()->allActors();
                for (int i = 0; i < actors.size(); i++) {
                    const auto balance =
                        node->blockchain()->calculate_actor_balance(ActorId(actors[i]), ActorId());
                    eInfo("[Actor: {}, balance: ", actors[i], balance);
                }
            }
        }
    }

    if (command.left(4) == "push") {
        command.replace(QRegularExpression("\\s+"), " ");
        QString      actorId = command.mid(5, 40);
        Notification notify { .time = 100, .type = Notification::NewPost, .data = actorId.toLatin1() + " " };
        m_pushManager->pushNotification(actorId, notify);
    }

    if (command.left(8) == "dfs add ") {
        auto                  file = command.mid(8).toStdWString();
        std::filesystem::path filepath(file);
        eInfo("Adding file to DFS: {}", command.mid(8));

        auto actor_id = node->accountController()->mainActor().id();
        auto result   = node->dfs()->store_file(actor_id,
                                              actor_id,
                                              file,
                                              "",
                                              filepath.filename().string(),
                                              Dfs::DataSecurity::Public);
        if (!result.has_value())
            return;

        if (!result.has_value()) {
            eInfo("Error: {}", result.error());
        }
    }

    if (command.left(8) == "dfs get ") {
        auto list = command.split(" ");
        if (list.size() < 4) {
            eInfo("List has less 2 parameters");
        } else {
            const std::string pathToNewFolder = list[2].toStdString();
            const std::string pathToDfsFile   = list[3].toStdString();
            if (pathToNewFolder.empty() || pathToDfsFile.empty()) {
                eLog("One or more parameters is empty. Please check in parameters.");
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
        auto exported = node->exportUser();
        if (!exported.has_value()) {
            eInfo("Can't export, error: {}", exported.error());
        }
        auto    data     = QString::fromStdString(exported.value());
        QString fileName = QString("%1.extrachain").arg(node->accountController()->mainActor().id().toQString());
        QFile   file(fileName);
        file.open(QFile::WriteOnly);
        if (file.write(data.toUtf8()) > 1)
            eInfo("Exported to {}", fileName);
        file.close();
    }

    if (command.left(16) == "list_user_files ") {
        auto userId = command.split(" ")[1];
        eInfo("show list user  {}  files", userId);
        std::filesystem::path actorFolderPath = DfsB::fsActrRoot + "/" + userId.toStdString();
        std::cout << "======================================================" << std::endl;

        for (const auto &entry : std::filesystem::recursive_directory_iterator(actorFolderPath)) {
            const auto fileName = entry.path().filename();
            if (fileName == "." || fileName == ".." || fileName == ".dir" || fileName == ".DS_Store")
                continue;

            if (!std::filesystem::is_directory(entry)) {
                const std::string filePath = actorFolderPath.string() + "/" + entry.path().filename().string();
                std::cout << entry.path() << std::endl;
            } else {
                std::cout << "------------------------------------------------------" << std::endl;
            }
        }
        std::cout << "======================================================" << std::endl;
    }
    // request_coins coins
    if (command.left(13) == "request_coins") {
        auto actorId = node->accountController()->mainActor().id();
        auto coins   = command.split(" ")[1];

        eInfo("Request coins:  {} for  {}", coins, actorId.toQString());
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
#ifdef Q_OS_WINDOWS
    DWORD consoleMode;
    bool  isInteractive = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &consoleMode);
    if (!isInteractive) {
        eLog("[Console] Console is not interactive, command input is disabled");
        return;
    }

    // if (IsDebuggerPresent()) {
    //     eLog("[Console] Input off, because debugger mode");
    //     return;
    // }

    connect(&consoleInput, &ConsoleInput::input, this, &ConsoleManager::commandReceiver);
    ThreadPool::addThread(&consoleInput);
#elif defined(Q_OS_UNIX)
    // connect(&notifier, &QSocketNotifier::activated, [this] {
    //     QString line = notifierInput.readLine();
    //     if (!line.isEmpty())
    //         commandReceiver(line);
    // });
#endif
}

void ConsoleManager::saveNotificationToken(QByteArray os, ActorId actorId, ActorId token) {
    m_pushManager->saveNotificationToken(os, actorId, token);
}

void ConsoleManager::dfsStart() {
    connect(node->dfs(), &DfsController::added, [](ActorId owner_id, Dfs::DirRow dirRow) {
        eInfo("[Console/Ddf] Added for {}: {}", owner_id, dirRow);
    });
    connect(node->dfs(), &DfsController::uploaded, [](ActorId owner_id, Dfs::DirRow dirRow) {
        eInfo("[Console/Ddf] Uploaded for {}: {}", owner_id, dirRow);
    });

    connect(node->dfs(), &DfsController::downloaded, [](ActorId owner_id, Dfs::DirRow dirRow) {
        eInfo("[Console/Ddf] Downloaded for {}: {}", owner_id, dirRow);
    });

    connect(node->dfs(),
            &DfsController::downloadProgress,
            [](ActorId owner_id, std::string file_id, int progress) {
                // eInfo("[Console/DFS] Download progress: {}/{}: {}", owner_id, file_id, progress);
            });

    connect(node->dfs(), &DfsController::uploadProgress, [](ActorId owner_id, std::string file_id, int progress) {
        // eInfo("[Console/DFS] Upload progress: {}/{}: {}", owner_id, file_id, progress);
    });
}

QString ConsoleManager::getSomething(const QString &name) {
    QString something;

    qInfo().noquote().nospace() << "Enter " + name + ":";

    QTextStream cin(stdin);
    while (something.isEmpty()) {
        something = cin.readLine();

        if (something.indexOf(" ") != -1) {
            eInfo("Please enter without spaces");
            something = "";
            continue;
        }
    }

    if (something == "wipe") {
        Utils::wipeDataFiles();
        eInfo("Wiped and exit...");
        std::exit(0);
    }

    if (something == "empty")
        something = "";

    return something;
}
