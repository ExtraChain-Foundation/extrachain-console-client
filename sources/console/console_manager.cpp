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

#include "managers/thread_pool.h"
#include "dfs/dfs_controller.h"
#include "chain/actor_index.h"
#include "managers/extrachain_node.h"
#include "utils/exc_logs.h"
#include "network/network_manager.h"
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
        Logger::instance().set_debug(true);
        Logger::start_file("extrachain");
        eInfo("Logs enabled");
    }

    if (command == "logs off") {
        Logger::instance().set_debug(false);
        Logger::stop_file();
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
        auto    mainActorId = node->account_controller()->system_actor().id();
        ActorId firstId     = node->actor_index()->network_id();

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

            Transaction tx;
            tx.set_sender(mainActorId);
            tx.set_receiver(receiver);
            tx.set_amount(amount);
            // createTransaction
            node->send_transaction(tx, node->account_controller()->system_actor());

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
            node->network()->connect_to_node(ip, networkProtocol);
        } else {
            eInfo("Invalid connect input");
        }
    }

    if (command.left(7) == "wallet ") {
        auto list = command.split(" ");
        if (list.length() > 1) {
            if (list[1] == "new") {
                auto actor = node->account_controller()->create_wallet();
                eInfo("Wallet created: {}", actor.id());
            }

            if (list[1] == "list") {
                eInfo("Wallets:");
                auto actors = node->account_controller()->accounts();
                auto mainId = node->account_controller()->system_actor().id();
                eInfo("User {}", mainId);
                for (const auto &actor : actors) {
                    if (actor.id() != node->account_controller()->system_actor().id()) {
                        eInfo("Wallet {}", actor.id());
                    }
                }
            }

            if (list[1] == "balance" || list[1] == "check_balance") {
                // const auto actors = node->actorIndex()->allActors();
                // for (int i = 0; i < actors.size(); i++) {
                //     const auto balance =
                //         node->blockchain()->calculate_actor_balance(ActorId(actors[i]), ActorId());
                //     eInfo("[Actor: {}, balance: ", actors[i], balance);
                // }
            }
        }
    }

    if (command.left(4) == "push") {
        command.replace(QRegularExpression("\\s+"), " ");
        QString actorId = command.mid(5, 40);
        // Notification notify { .time = 100, .type = Notification::NewPost, .data = actorId.toLatin1() + " " };
        // m_pushManager->pushNotification(actorId, notify);
    }

    if (command.left(8) == "dfs add ") {
        auto                  file = command.mid(8).toStdWString();
        std::filesystem::path filepath(file);
        eInfo("Adding file to DFS: {}", command.mid(8));

        auto actor_id = node->account_controller()->system_actor().id();
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

            // if (list.size() == 5) {
            //     node->dfs()->exportFile(pathToNewFolder, pathToDfsFile, list[4].toStdString());
            // } else if (list.size() == 4) {
            //     node->dfs()->exportFile(pathToNewFolder, pathToDfsFile);
            // }
        }
    }

    if (command.left(6) == "export") {
        auto exported = node->export_profile();
        if (!exported.has_value()) {
            eInfo("Can't export, error: {}", exported.error());
        }
        auto    data = QString::fromStdString(exported.value());
        QString fileName =
            QString("%1.extrachain").arg(node->account_controller()->system_actor().id().toQString());
        QFile file(fileName);
        file.open(QFile::WriteOnly);
        if (file.write(data.toUtf8()) > 1)
            eInfo("Exported to {}", fileName);
        file.close();
    }

    if (command.left(16) == "list_user_files ") {
        auto userId = command.split(" ")[1];
        eInfo("show list user  {}  files", userId);
        std::filesystem::path actorFolderPath = DfsB::DFS_FOLDER + "/" + userId.toStdString();
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

    if (command.startsWith("account_settings")) {
        if (command.contains("change_password")) {
            QStringList l = command.split(" ");
            if (l.size() == 6) {
                std::string currentLogin    = l[2].toStdString();
                std::string currentPassword = l[3].toStdString();
                std::string newLogin        = l[4].toStdString();
                std::string newPassword     = l[5].toStdString();

                eInfo("Entered parameters: current_login={}, current_password={}, new_login={}, new_password={}",
                      currentLogin,
                      currentPassword,
                      newLogin,
                      newPassword);

                const bool result = node->account_controller()->change_credentials(currentLogin,
                                                                                   currentPassword,
                                                                                   newLogin,
                                                                                   newPassword);
                if (result) {
                    eInfo("Credentials changed.");
                } else {
                    eWarning("Credentials doesn't changed.");
                }
            } else {
                eWarning("Invalid arguments. Expected 6 parameters, got {}", l.size());
                eInfo(
                    "Usage: account_settings change_password <current_login> <current_password> <new_login> "
                    "<new_password>");
                eInfo("Example: account_settings change_password john old_pass123 john new_pass456");
            }
        }

        if (command.contains("change_username")) {
            QStringList l = command.split(" ");

            if (l.size() == 3) {
                QString newUserName = l[2];
                if (newUserName.length() <= 5) {
                    eWarning("Invalid username. Username length must be more 5 characters.");
                    return;
                }

                if (newUserName.length() >= 30) {
                    eWarning("Invalid username. Username must be under 30 characters");
                    return;
                }

                static const QRegularExpression re("^[A-Za-z_][A-Za-z0-9_]*$");
                if (!re.match(newUserName).hasMatch()) {
                    eWarning("Invalid username. Username can only contain letters, numbers, underscore and dot");
                    return;
                }

                std::vector<std::string> domainEndings = { ".com",  ".org",    ".net",  ".io",  ".app", ".dev",
                                                           ".co",   ".me",     ".info", ".biz", ".tv",  ".cc",
                                                           ".site", ".online", ".ai",   ".ua" };
                for (int i = 0; i < domainEndings.size(); i++) {
                    if (newUserName.endsWith(QString::fromStdString(domainEndings[i]), Qt::CaseInsensitive)) {
                        eWarning("Invalid username. Username cannot look like a domain name");
                        return;
                    }
                }

                std::vector<std::string> forbiddenWords     = { "extrachain" };
                std::vector<std::string> forbiddenPatterns  = { "rac+o+n", "fu+c+k",  "sh+i+t", "da+mn",
                                                                "di+c+k",  "co+c+k",  "cu+nt",  "bi+tc+h",
                                                                "ro+o+t",  "a+dmi+n", "syste+m" };
                std::vector<std::string> impersonationWords = {
                    "moderator", "support",   "official", "staff",  "help",     "service", "google",
                    "facebook",  "microsoft", "apple",    "amazon", "twitter",  "openai",  "anthropic",
                    "tesla",     "nvidia",    "chatgpt",  "claude", "gemini",   "copilot", "assistant",
                    "bitcoin",   "ethereum",  "binance",  "kraken", "coinbase", "whitebit"
                };

                QString normalizedUsername = newUserName.toLower().remove(QRegularExpression("[._]"));
                for (int ii = 0; ii < forbiddenWords.size(); ii++) {
                    if (normalizedUsername.contains(forbiddenWords[ii].c_str())) {
                        eWarning("Invalid username. Username contains forbidden words");
                        return;
                    }
                }

                for (int j = 0; j < forbiddenPatterns.size(); j++) {
                    QRegularExpression pattern = QRegularExpression(forbiddenPatterns[j].c_str(),
                                                                    QRegularExpression::CaseInsensitiveOption);
                    if (pattern.match(newUserName).hasMatch()) {
                        eWarning("Invalid username. Username contains forbidden words");
                        return;
                    }
                }

                for (int k = 0; k < impersonationWords.size(); k++) {
                    if (normalizedUsername.contains(impersonationWords[k].c_str())) {
                        eWarning("Invalid username. Username cannot contain official or brand names");
                        return;
                    }
                }

                auto network_id = node->actor_index()->network_id();
                if (network_id.is_zero()) {
                    QFile network(".network_id");
                    if (network.open(QFile::ReadOnly)) {
                        auto data = network.readAll();
                        network.close();

                        if (!data.isEmpty()) {
                            auto actor_id = ActorId::create(data.toStdString());
                            if (actor_id.has_value()) {
                                network_id = actor_id.value();
                            } else {
                                return;
                            }
                        }
                    }
                }

                if (network_id.is_zero()) {
                    return;
                }

                std::string usernames_file_id;
                bool        m_usernameActive = false;
                auto        search_result =
                    Dfs::Tables::DirsFile::ActorSpace::search_file_by_folder_and_name(node->dfs()
                                                                                          ->get_db_instance(),
                                                                                      network_id,
                                                                                      Dfs::Basic::TEMPLATE_VECTOR,
                                                                                      "Usernames");
                auto db_instance    = node->dfs()->dirs_manager().get_db_instance();
                auto search_result2 = Dfs::Tables::DirsFile::ActorSpace::
                    search_file_by_folder_and_name(db_instance,
                                                   network_id,
                                                   Dfs::Basic::TEMPLATE_COLLECTION_TEMPLATE,
                                                   "Usernames");
                if (!search_result.has_value()) {
                    return;
                }
                if (!search_result2.has_value()) {
                    return;
                }

                if (search_result->state == Dfs::FileState::Ready
                    && search_result2->state == Dfs::FileState::Ready) {
                    usernames_file_id = search_result->file_id;
                    m_usernameActive  = true;
                }

                if (search_result->state != Dfs::FileState::Ready
                    || search_result2->state != Dfs::FileState::Ready) {
                    return;
                }

                auto v = DfsVector::load(node,
                                         node->account_controller()->system_actor(),
                                         network_id,
                                         usernames_file_id);
                if (!v.has_value()) {
                    return;
                }

                auto main_id = node->account_controller()->current_profile().main_id();
                auto row =
                    v->read_rows(fmt::format("WHERE name='{}' COLLATE NOCASE AND status = '1' AND actor != '{}'",
                                             newUserName.toStdString(),
                                             main_id));

                if (row.has_value()) {
                    eWarning("Invalid username. Username already exists");
                    return;
                }

                auto res = node->dfs()->add_vector_row(network_id,
                                                       usernames_file_id,
                                                       {
                                                           { "name", newUserName.toStdString() },
                                                       });

                if (res) {
                    eInfo("Username saved successfully");
                } else {
                    eInfo("Error saved username");
                }
            } else {
                eWarning("Invalid arguments. Expected 3 parameters, got {}", l.size());
                eInfo("Usage: account_settings change_username <new_username>");
                eInfo("Example: account_settings change_username john7travolta");
            }
        }

        if (command.contains("export")) {
            QStringList l = command.split(" ");
            if (l.size() > 2 && l.size() <= 5) {
                QString type     = l[2];
                auto    mnemonic = node->account_controller()->seed_mnemonic();

                QStringList phrases;
                for (const auto &word : mnemonic) {
                    phrases << QString::fromStdString(word);
                }

                if (type == "file") {
                    if (l.size() != 5) {
                        eWarning("Invalid arguments. For file expected 5 parameters, got {}", l.size());
                    } else {
                        QString   savedPath = l[3];
                        QString   nameFile  = l[4];
                        QFileInfo info(savedPath);
                        if (info.isDir()) {
                            eInfo("Try to save in {}", savedPath);
                            std::expected<std::string, ImportError> res = node->export_profile();
                            if (!res.has_value()) {
                                QString errorText =
                                    tr("Export operation failed: ")
                                    + QString::fromStdString(Utils::enum_value_name_value(res.error()));

                                eInfo("Error export file: {}", errorText);
                            }

                            QString filePath = QString("%1/%2").arg(savedPath, nameFile);
                            auto    fs_path  = FsPath::create(filePath.toStdString());
                            if (!fs_path.has_value()) {
                                eWarning("Export operation failed: file path");
                                return;
                            }
                            std::ofstream file(fs_path->native(), std::ios::binary);
                            if (!file) {
                                eWarning("Export operation failed: file access");
                                return;
                            }

                            auto fileContent = node->account_controller()->profile_type() == ProfileType::New
                                                   ? res.value()
                                                   : Utils::to_base64(res.value());

                            if (!file.write(fileContent.c_str(), fileContent.size())) {
                                eWarning("Export operation failed: file write");
                                return;
                            } else {
                                eWarning("Exported as file. {}", filePath);
                            }
                            file.close();
                        } else {
                            eWarning("Invalid. This is not folder.");
                        }
                    }
                } else if (type == "phrase") {
                    eInfo("{}", phrases);
                } else if (type == "hex") {
                    const std::string generatedHex = node->account_controller()->seed_hex();
                    auto              hex          = QString::fromStdString(generatedHex);
                    eInfo("HEX: [{}]", hex);
                } else {
                    eWarning("Invalid arguments. Expected file/phrase/hex");
                }
            } else {
                eWarning("Invalid arguments. Expected 3 parameters, got {}", l.size());
                eInfo("Usage: account_settings export <type>");
                eInfo("Example: account_settings export file/phrase/hex");
            }
        }
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
    ThreadPool::add_thread(&consoleInput);
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
        eLog("[Console/Dfs] Added for {}: {}", owner_id, dirRow);
    });
    connect(node->dfs(), &DfsController::uploaded, [](ActorId owner_id, Dfs::DirRow dirRow) {
        eLog("[Console/Dfs] Uploaded for {}: {}", owner_id, dirRow);
    });

    connect(node->dfs(), &DfsController::downloaded, [](ActorId owner_id, Dfs::DirRow dirRow) {
        eLog("[Console/Dfs] Downloaded for {}: {}", owner_id, dirRow);
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
