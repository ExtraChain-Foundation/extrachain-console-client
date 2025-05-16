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

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLockFile>
#include <QStandardPaths>

#include <csignal>

#include "extrachain_version.h"
#include "utils/exc_utils.h"
#include "console/console_manager.h"
#include "managers/extrachain_node.h"
#include "managers/logs_manager.h"
#include "utils/exc_logs.h"
#include "metatypes.h"

#ifdef Q_OS_LINUX
    #include <execinfo.h>
#endif

#ifdef Q_OS_WINDOWS
    #include <windows.h>
    #include <dbghelp.h>
    #pragma comment(lib, "dbghelp.lib")
#endif

#ifndef EXTRACHAIN_CMAKE
    #include "preconfig.h"
#endif

#ifdef Q_OS_WIN
const char* strsignal(int sig) {
    static const std::unordered_map<int, const char*> sig2NameMap = { { SIGINT, "SIGINT" },
                                                                      { SIGTERM, "SIGTERM" },
                                                                      { SIGABRT, "SIGABRT" },
                                                                      { SIGSEGV, "SIGSEGV" } };

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
            eCritical("Signal achieved: {}", strsignal(sig));
            QCoreApplication::quit();
            break;
        }
        case SIGABRT:
        case SIGSEGV: {
            eCritical("Catch signal: {}", strsignal(sig));
#ifdef Q_OS_WIN
            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_DEBUG);
            if (!SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
                eCritical("Failed to initialize symbol handler. Error: {}", GetLastError());
            } else {
                constexpr int maxFrames = 64;
                void*         stackTrace[maxFrames];
                WORD          frames = CaptureStackBackTrace(0, maxFrames, stackTrace, nullptr);
                SYMBOL_INFO*  symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
                symbol->MaxNameLen   = 255;
                symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                IMAGEHLP_LINE64 line;
                line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

                for (WORD i = 0; i < frames; i++) {
                    DWORD64 address = (DWORD64)(stackTrace[i]);
                    DWORD   displacement;
                    if (SymFromAddr(GetCurrentProcess(), address, 0, symbol)) {
                        if (SymGetLineFromAddr64(GetCurrentProcess(), address, &displacement, &line)) {
                            eInfo("{}",
                                  QString("#%1: %2 in %3 : line %4")
                                      .arg(i)
                                      .arg(symbol->Name)
                                      .arg(line.FileName)
                                      .arg(line.LineNumber));
                        } else {
                            eInfo("{}",
                                  QString("#%1: %2 - 0x%3 (Failed to get line info, error: %4)")
                                      .arg(i)
                                      .arg(symbol->Name)
                                      .arg(QString::number(address, 16))
                                      .arg(GetLastError()));
                        }
                    } else {
                        eInfo("{}",
                              QString("#%1: 0x%2 (Failed to get symbol, error: %3)")
                                  .arg(i)
                                  .arg(QString::number(address, 16))
                                  .arg(GetLastError()));
                    }
                }
                free(symbol);
            }
#elif defined(Q_OS_LINUX)
            void*  stackTrace[64];
            int    frameCount = backtrace(stackTrace, 64);
            char** symbols    = backtrace_symbols(stackTrace, frameCount);
            if (symbols != nullptr) {
                for (int i = 0; i < frameCount; ++i)
                    eInfo("# {} : {}", i, symbols[i]);
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
    } catch (const std::exception& ex) {
        eCritical("SignalHandler exception: {}", ex.what());
        _exit(EXIT_FAILURE);
    }
}

bool SetupSignals() {
#ifdef Q_OS_WIN
    constexpr const std::array<int, 4> arrSig = { { SIGINT, SIGTERM, SIGSEGV, SIGABRT } };
#else
    constexpr const std::array<int, 5> arrSig = { { SIGINT, SIGTERM, SIGSEGV, SIGABRT, SIGQUIT } };
#endif

    for (const auto& item : arrSig) {
        if (signal(item, HandleSignal) == SIG_ERR) {
            eCritical("{}",
                      "Cannot handle a signal: " + QString::fromStdString(strsignal(item)) + ", reason - "
                          + QString::fromStdString(strerror(errno)));
            return false;
        }
    }

    return true;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    app.setApplicationName("ExtraChain Console Client");
    app.setOrganizationName("ExtraChain Foundation");
    app.setOrganizationDomain("https://extrachain.io/");
    app.setApplicationVersion(QString::fromStdString(extrachain_version));

    if (!SetupSignals())
        exit(EXIT_FAILURE);

    registerMetaTypes();

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    static QLockFile lockFile(".extrachain-console.lock");
    if (!lockFile.tryLock(100)) {
        fmt::println("ExtraChain Console Client already running in directory {}", QDir::currentPath());
        std::exit(0);
    }
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "ExtraChain (ExC) is a lightweight blockchain infrastructure and decentralized storage ExDFS "
        "allowing creation of high-load dApps (decentralized applications) for both portable, "
        "non-portable and IoT devices.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption debugOption("debug", "Enable debug logs");
    QCommandLineOption clearDataOption("clear-data", "Wipe all data");
    QCommandLineOption dirOption("current-dir", "Set current directory.", "current-dir");
    QCommandLineOption emailOption({ "e", "email" }, "Set email", "email");
    QCommandLineOption passOption({ "s", "password" }, "Set password", "password");
    QCommandLineOption inputOption("disable-input", "Console input disable");
    QCommandLineOption core("core", "First network creation");
    QCommandLineOption dag_genesis("dag-genesis", "First dag creation");
    QCommandLineOption importOption("import", "Import from file", "import");
    QCommandLineOption netdebOption("network-debug", "Print all messages. Only for debug build");
    QCommandLineOption dfsLimitOption({ "l", "limit" }, "Set limit", "dfs-limit");
    QCommandLineOption blockDisableCompress("disable-compress", "Blockchain compress disable");
    QCommandLineOption megaOption("mega", "Create mega loot");
    QCommandLineOption tokenOption("create-token-cache", "Create token cache for network id");
    QCommandLineOption usernamesOption("create-usernames", "Create usernames vector from network id");
    QCommandLineOption subscriptionOption("create-subscription-template",
                                          "Create subscription template from network id");
    QCommandLineOption chatOption("create-chat-templates", "Create chat templates from network id");

    QCommandLineOption megaImportOption("import-from-mega", "Import from console-data/0 file");

    parser.addOptions({ debugOption,
                        dirOption,
                        emailOption,
                        passOption,
                        inputOption,
                        core,
                        dag_genesis,
                        clearDataOption,
                        importOption,
                        netdebOption,
                        dfsLimitOption,
                        blockDisableCompress,
                        // megaOption,
                        tokenOption,
                        usernamesOption,
                        subscriptionOption,
                        chatOption,
                        megaImportOption });
    parser.process(app);

    // TODO: allow absolute directory
    QString dirName = Utils::fixFileName(parser.value(dirOption), "");
    Utils::dataDir(dirName.isEmpty() ? "console-data" : dirName);
    QDir().mkdir(Utils::dataDir());
    QDir::setCurrent(QDir::currentPath() + QDir::separator() + Utils::dataDir());
    Logger::start_file();

    if (parser.isSet(clearDataOption) || parser.isSet(core)) {
        Utils::wipeDataFiles();

#ifndef QT_DEBUG
        eCritical("You need to remove the console-data folder manually");
        std::exit(0);
#endif
    }

    //    QLockFile lockFile(".console.lock");
    //    if (!lockFile.tryLock(100)) {
    //        if (!QFile::exists(".console.lock")) {
    //            eLog("[Console] Unable to write files. Check folder permissions");
    //            return -1;
    //        }
    //        eLog("[Console] Already running in directory {}", QDir::currentPath());
    //        return -1;
    //    }

    LogsManager::debugLogs = parser.isSet(debugOption);
#ifdef QT_DEBUG
    LogsManager::debugLogs = !parser.isSet(debugOption);
    Network::networkDebug  = parser.isSet(netdebOption);
    eInfo("Debug logs enabled: {}", LogsManager::debugLogs);
#endif
    Logger::instance().set_debug(LogsManager::debugLogs);

    LogsManager::onFile();

    eInfo(" ┌───────────────────────────────────────────┐");
    eInfo(" │          ExtraChain {}.{}          │", extrachain_version, COMPILE_DATE);
    if (LogsManager::debugLogs)
        eInfo(" │     Console: {} | Core: {}     │", GIT_COMMIT, GIT_COMMIT_CORE);
    eInfo(" └───────────────────────────────────────────┘");
    LogsManager::etHandler();
    qInfo().noquote().nospace() << "[Build Info] " << Utils::detectCompiler() << ", Qt " << QT_VERSION_STR
                                << ", SQLite " << DbConnector::sqlite_version() << ", Sodium "
                                << Utils::sodiumVersion().c_str() << ", Boost " << Utils::boostVersion();
    // << ", Boost Asio " << Utils::boostAsioVersion();
    if (QString(GIT_BRANCH) != "dev" || QString(GIT_BRANCH_CORE) != "dev")
        qInfo().noquote() << "[Branches] Console:" << GIT_BRANCH << "| ExtraChain Core:" << GIT_BRANCH_CORE;
    eInfo("");
    eLog("[Console] Debug logs on");

    bool           isNewNetwork = parser.isSet(core);
    ConsoleManager console;
    if (!dirName.isEmpty())
        eLog("Custom data directory: {}", dirName);

    if (LogsManager::debugLogs)
        LogsManager::print("");

    QString argEmail    = parser.value(emailOption);
    QString argPassword = parser.value(passOption);
    QString email =
        argEmail.isEmpty() && !AutologinHash::isAvailable() ? ConsoleManager::getSomething("e-mail") : argEmail;
    QString password = argPassword.isEmpty() && !AutologinHash::isAvailable()
                           ? ConsoleManager::getSomething("password")
                           : argPassword;
    if (argEmail.isEmpty() || argPassword.isEmpty())
        LogsManager::print("");
    if (parser.isSet(inputOption))
        eLog("[Console] Input off");
    else
        console.startInput();

    ExtraChainNodeWrapper* nodeWrapper = new ExtraChainNodeWrapper(&app);
    auto                   node        = nodeWrapper->node;
    nodeWrapper->Init(true);

    if (parser.isSet(blockDisableCompress)) {
        // node->blockchain()->getBlockIndex().setBlockCompress(false);
    }

    QObject::connect(node, &ExtraChainNode::NodeInitialised, [&]() {
        eLog("[Console] Activated");
        console.setExtraChainNode(node);
        console.dfsStart();

        QString dfsLimit = parser.value(dfsLimitOption);
        if (!dfsLimit.isEmpty()) {
            bool    isOk  = false;
            quint64 limit = dfsLimit.toULongLong(&isOk);
            if (isOk) {
                // node->dfs()->setBytesLimit(limit);
            }
        }

        if (isNewNetwork) {
            bool res = node->create_new_network(email.toStdString(), password.toStdString());
            if (!res) {
                eInfo("Can't create new network");
                std::exit(0);
            }
        }

        QString importFile = parser.value(importOption);
        if (!importFile.isEmpty()) {
            QFile file(importFile);
            file.open(QFile::ReadOnly);
            auto data = file.readAll().toStdString();
            if (data.empty()) {
                eInfo("Incorrect import");
                std::exit(0);
            }
            node->import_profile(data, email.toStdString(), password.toStdString());
            file.close();
        }

        if (node->accountController()->count() == 0) {
            std::string   loginHash;
            AutologinHash autologinHash;
            if (AutologinHash::isAvailable() && autologinHash.load()) {
                loginHash = autologinHash.hash();
            } else {
                loginHash = Utils::calculate_hash((email + password).toStdString());
                password.clear();
            }

            auto result = node->login(loginHash);
            if (!result) {
                if (AccountController::profilesList().size() != 0)
                    eInfo("Error: Incorrect login or password");
                else
                    eInfo("Error: No profiles files");
                std::exit(-1);
            }
        }

        if (parser.isSet(dag_genesis)) {
            node->create_new_dag();
        }

        //
        bool is_token = parser.isSet(tokenOption);
        if (is_token || isNewNetwork) {
            bool res1 = node->create_token_template();
            if (res1) {
                eSuccess("Tokens cache template created");

                bool res2 = node->create_token_vector();
                if (res2) {
                    eSuccess("Tokens cache vector created");
                } else {
                    eInfo("Can't create tokens cache template");
                }
            } else {
                eInfo("Can't create tokens cache vector");
            }
        }

        bool is_username = parser.isSet(usernamesOption);
        if (is_username || isNewNetwork) {
            auto res = node->create_usernames_vector();
            if (!res) {
                eInfo("Can't create usernames vector");
            } else
                eSuccess("Usernames vector created");
        }

        bool subscription_create = parser.isSet(subscriptionOption);
        if (subscription_create || isNewNetwork) {
            auto res = node->create_subscription_template();

            // temp
            // node->create_subscription_vector("TestSubscription");

            if (!res) {
                eInfo("Can't create subscription template");
            } else {
                eSuccess("Subscription template created");
            }
        }

        bool chat_create = parser.isSet(chatOption);
        if (chat_create || isNewNetwork) {
            auto res = node->create_chat_templates();

            if (!res) {
                eInfo("Can't create chat templates");
            } else {
                eSuccess("Chat templates created");
            }
        }

        bool is_mega = false; // parser.isSet(megaOption);
        if (is_mega) {
            /*
            Logger::instance().set_debug(true);
            auto mega = node->blockchain()->create_mega_genesis_block(node->accountController()->system_actor());

            if (!mega.has_value()) {
                eLog("[MEGA] Error {}", mega.error());
                qApp->exit();
                return;
            }

            eInfo("[MEGA] Data rows size: {}", mega->dataRows().size());
            eInfo("[MEGA Remove old blockchain");
            node->blockchain()->removeAllSlot(true);
            eInfo("[MEGA] Create new zero block");
            node->blockchain()->getBlockIndex().addBlock(mega.value());
            qApp->exit();
            */
        }

        bool is_mega_import = parser.isSet(megaImportOption);
        if (is_mega_import) {
            if (node->dag()->current_section() != BigNumber(0)) {
                eFatal("Last section must be 0");
            }

            DbConnector db("0");
            if (!db.open()) {
                eFatal("Can't open console-data/0 mega block");
            }

            auto rows          = db.select("SELECT * FROM GenesisDataRow");
            auto network_actor = node->accountController()->currentProfile().get_actor(node->network_id()).value();
            auto section       = node->dag()->read_section(BigNumber(0));

            if (!section.has_value()) {
                eFatal("No zero section");
                return;
            }

            for (auto& row : rows) {
                Transaction tx;
                tx.setSender(node->network_id());
                tx.setReceiver(ActorId(row["actorId"]));
                tx.setAmount(BigNumberFloat(row["state"]));
                tx.setToken(ActorId(row["token"]));
                tx.setType(TransactionType::Balance);
                tx.set_section(BigNumber(1));

                if (section.has_value()) {
                    tx.set_prev_hashs(section->hashs());
                }

                tx.set_timestamp(Utils::current_date_ms());

                auto sign_res = tx.sign(network_actor.get());
                if (!sign_res) {
                    eFatal("Sign balance error");
                    return;
                }

                Responder responder(node->network());
                node->dag()->save_transaction(tx);
            }
        }
        return;
    });

    return app.exec();
}
