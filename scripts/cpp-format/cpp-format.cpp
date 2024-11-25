#include <QCoreApplication>
#include <QDebug>
#include <QDirIterator>
#include <QProcess>

void runClangFormat(const QString &path) {
    QDirIterator it(path, { "*.cpp", "*.h" }, QDir::NoFilter, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        auto file = it.next();

        int res = QProcess::execute("clang-format", { "-i", file, "-style=file" });
        qInfo().noquote() << "Format" << QString(file).replace(path + "/", "")
                          << (res == 0 ? "finished" : "error");
    }
}

int specificPathSearch(const QString &path) {
    qInfo().noquote() << "Format in path:" << QDir(path).path();

    if (!QFile::exists(path + "/_clang-format")) {
        qInfo() << "You need _clang-format file at the root";
        return -2;
    }

    runClangFormat(path);

    qInfo().noquote() << "End" << path;
    return 0;
}

int searchFromCurrentPath() {
    qInfo() << "Can't find path from args";
    auto dir = QDir::current();

    if (!dir.path().contains("/extrachain-console-client/")) {
        return -1;
    }

    qInfo().noquote() << "Search in the current path:" << dir.path();

    const auto folders = QStringList { "extrachain-core", "extrachain-console-client", "extrachain-ui-client" };
    while (dir.cdUp()) {
        const auto dirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

        for (const auto &el : dirs) {
            for (const auto &folder : folders) {
                if (el == folder) {
                    auto path = dir.path() + "/" + folder;
                    qInfo().noquote() << path;
                    specificPathSearch(path);
                }
            }
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    if (argc < 2) {
        return searchFromCurrentPath();
    } else {
        auto path = QDir(argv[1]).path();
        return specificPathSearch(path);
    }
}
