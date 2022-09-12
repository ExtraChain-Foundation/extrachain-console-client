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

#include "wasm3/wasm3_cpp.h"
#include "wasm3/test_prog.wasm.h"

#ifndef EXTRACHAIN_CMAKE
    #include "preconfig.h"
#endif


int sum(int a, int b)
{
    return a + b;
}

void * ext_memcpy (void* dst, const void* arg, int32_t size)
{
    return memcpy(dst, arg, (size_t) size);
}

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
    QCommandLineOption netdebOption("network-debug", "Print all messages. Only for debug build");
    QCommandLineOption dfsLimitOption("dfs-limit", "Temp", "dfs-limit");
    parser.addOptions({ debugOption, dirOption, emailOption, passOption, inputOption, createNetworkOption,
                        clearDataOption, importOption, netdebOption, dfsLimitOption });
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
    Network::networkDebug = parser.isSet(netdebOption);
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
    QString email = argEmail.isEmpty() && !AutologinHash::isAvailable()
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
        bool isOk = false;
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
        std::string loginHash;
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
    }

    QObject::connect(&node, &ExtraChainNode::ready, [&]{
        //start web assembly test
        std::cout << "Loading WebAssembly..." << std::endl;

        /* Wasm module can be loaded from a file */
        try {
            wasm3::environment env = wasm3::environment();
            wasm3::runtime runtime = env.new_runtime(1024);
            //fill in path to test_prog file
            const char* file_name = "test_prog.wasm";
            std::ifstream wasm_file(file_name, std::ios::binary | std::ios::in);
            if (!wasm_file.is_open()) {
                throw std::runtime_error("Failed to open wasm file");
            }
            wasm3::module mod = env.parse_module(wasm_file);
            runtime.load(mod);
        }
        catch(std::runtime_error &e) {
            std::cerr << "WASM3 error: " << e.what() << std::endl;
            return 1;
        }

        /* Wasm module can also be loaded from an array */
        try {
            wasm3::environment env;
            wasm3::runtime runtime = env.new_runtime(1024);
            wasm3::module mod = env.parse_module(test_prog_wasm, test_prog_wasm_len);
            runtime.load(mod);

            mod.link("*", "sum", sum);
            mod.link("*", "ext_memcpy", ext_memcpy);

            {
                wasm3::function test_fn = runtime.find_function("test");
                auto res = test_fn.call<int>(20, 10);
                std::cout << "result: " << res << std::endl;
            }
            {
                wasm3::function memcpy_test_fn = runtime.find_function("test_memcpy");
                auto res = memcpy_test_fn.call<int64_t>();
                std::cout << "result: 0x" << std::hex << res << std::dec << std::endl;
            }

            /**
             * Calling functions that modify an internal state, with mixed argument / return types
             */
            {
                wasm3::function counter_get_fn = runtime.find_function("test_counter_get");
                wasm3::function counter_inc_fn = runtime.find_function("test_counter_inc");
                wasm3::function counter_add_fn = runtime.find_function("test_counter_add");

                     // call with no arguments and a return value
                auto value = counter_get_fn.call<int32_t>();
                std::cout << "counter: " << value << std::endl;

                     // call with no arguments and no return value
                counter_inc_fn.call();
                value = counter_get_fn.call<int32_t>();
                std::cout << "counter after increment: " << value << std::endl;

                     // call with one argument and no return value
                counter_add_fn.call(42);
                value = counter_get_fn.call<int32_t>();
                std::cout << "counter after adding value: " << value << std::endl;
            }
        }
        catch(wasm3::error &e) {
            std::cerr << "WASM3 error: " << e.what() << std::endl;
            return 1;
        }
    });

    return app.exec();
}
