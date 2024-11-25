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
