#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <QCoreApplication>
#include <map>
#include <string>
#include <iostream>
#include <expected>         // для std::expected
#include "blockchain/dag.h" // Предполагаем, что ваша структура Section находится здесь

// Функция для получения относительного пути к файлу, начиная с 'dag/'
QString getRelativeDagPath(const QString& absolutePath, const QString& basePath) {
    // Поиск подстроки "dag/" в абсолютном пути
    int dagIndex = absolutePath.indexOf("/dag/");

    if (dagIndex != -1) {
        // Возвращаем путь начиная с "dag/"
        return absolutePath.mid(dagIndex + 1); // +1 чтобы убрать первый слеш
    }

    // Пробуем найти "dag" в начале пути
    dagIndex = absolutePath.indexOf("dag/");
    if (dagIndex != -1) {
        // Возвращаем путь начиная с "dag/"
        return absolutePath.mid(dagIndex);
    }

    // Если "dag/" не найдено, пытаемся получить относительный путь от базовой директории
    QString relativePath = QDir(basePath).relativeFilePath(absolutePath);
    if (relativePath.startsWith("dag/") || relativePath.indexOf("/dag/") != -1) {
        // Если в относительном пути есть "dag/", возвращаем подстроку
        int idx = relativePath.indexOf("/dag/");
        if (idx != -1) {
            return relativePath.mid(idx + 1);
        }
        // или просто возвращаем его как есть, если он начинается с "dag/"
        return relativePath;
    }

    // Если ничего не нашли, возвращаем пустую строку
    qWarning() << "Could not determine relative dag path for:" << absolutePath;
    return QString();
}

// Рекурсивная функция для обхода директории и обработки файлов
void processDirectory(const QString& dirPath, std::map<std::string, Section>& mergedSections) {
    fprintf(stderr, "Processing directory: %s\n", dirPath.toStdString().c_str());

    QDir          dir(dirPath);
    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);

    fprintf(stderr, "Found %d entries in directory\n", entries.size());

    for (const QFileInfo& entry : entries) {
        if (entry.isDir()) {
            // Рекурсивно обрабатываем поддиректорию
            fprintf(stderr, "Found subdirectory: %s\n", entry.absoluteFilePath().toStdString().c_str());
            processDirectory(entry.absoluteFilePath(), mergedSections);
        } else if (entry.isFile()) {
            fprintf(stderr, "Processing file: %s\n", entry.absoluteFilePath().toStdString().c_str());

            // Получаем относительный путь к файлу
            QString relativePath = getRelativeDagPath(entry.absoluteFilePath(), dirPath);

            fprintf(stderr, "Relative path: %s\n", relativePath.toStdString().c_str());

            if (!relativePath.isEmpty()) {
                // Обрабатываем файл
                QFile file(entry.absoluteFilePath());
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QByteArray data = file.readAll();
                    file.close();

                    fprintf(stderr, "File read, size: %d bytes\n", data.size());

                    std::string content = data.toStdString();

                    // Используем std::expected для десериализации
                    fprintf(stderr, "Attempting to deserialize...\n");

                    try {
                        std::expected<Section, std::string> result = Json::deserialize<Section>(content);

                        if (result.has_value()) {
                            Section section = result.value();

                            fprintf(stderr,
                                    "Deserialization successful, transactions: %zu\n",
                                    section.transactions.size());

                            // Добавляем или объединяем секцию
                            std::string pathStr = relativePath.toStdString();
                            if (mergedSections.find(pathStr) == mergedSections.end()) {
                                // Просто сохраняем секцию, если это первый файл с таким путем
                                mergedSections[pathStr] = section;
                                fprintf(stderr,
                                        "Added new section from file: %s to path: %s\n",
                                        entry.absoluteFilePath().toStdString().c_str(),
                                        relativePath.toStdString().c_str());
                            } else {
                                // Объединяем транзакции
                                size_t beforeSize = mergedSections[pathStr].transactions.size();
                                for (const auto& transaction : section.transactions) {
                                    mergedSections[pathStr].transactions.insert(transaction);
                                }
                                size_t afterSize = mergedSections[pathStr].transactions.size();

                                fprintf(stderr,
                                        "Merged section from file: %s with existing path: %s. Added %zu unique "
                                        "transactions\n",
                                        entry.absoluteFilePath().toStdString().c_str(),
                                        relativePath.toStdString().c_str(),
                                        afterSize - beforeSize);
                            }
                        } else {
                            // Обработка ошибки десериализации
                            fprintf(stderr,
                                    "Failed to deserialize file: %s Error: %s\n",
                                    entry.absoluteFilePath().toStdString().c_str(),
                                    result.error().c_str());
                        }
                    } catch (const std::exception& e) {
                        fprintf(stderr, "Exception during deserialization: %s\n", e.what());
                    } catch (...) {
                        fprintf(stderr, "Unknown exception during deserialization\n");
                    }
                } else {
                    fprintf(stderr,
                            "Failed to open file: %s Error: %s\n",
                            entry.absoluteFilePath().toStdString().c_str(),
                            file.errorString().toStdString().c_str());
                }
            }
        }
    }
}

// Вспомогательная функция для сохранения секции в файл
bool saveSection(const Section& section, const QString& filePath) {
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::string jsonData     = Json::serialize(section);
        qint64      bytesWritten = file.write(jsonData.c_str(), jsonData.length());
        file.close();
        return bytesWritten > 0;
    } else {
        fprintf(stderr,
                "Failed to write file: %s Error: %s\n",
                filePath.toStdString().c_str(),
                file.errorString().toStdString().c_str());
        return false;
    }
}

// Функция для обработки директории и сохранения файлов напрямую
void processSaveDirectory(const QString& sourceDir, const QString& resultDir) {
    fprintf(stderr, "Processing and saving from directory: %s\n", sourceDir.toStdString().c_str());

    QDir          dir(sourceDir);
    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);

    fprintf(stderr, "Found %d entries in directory\n", entries.size());

    for (const QFileInfo& entry : entries) {
        if (entry.isDir()) {
            // Рекурсивно обрабатываем поддиректорию
            processSaveDirectory(entry.absoluteFilePath(), resultDir);
        } else if (entry.isFile()) {
            // Получаем относительный путь к файлу
            QString relativePath = getRelativeDagPath(entry.absoluteFilePath(), sourceDir);

            if (!relativePath.isEmpty()) {
                // fprintf(stderr,
                //         "Processing file: %s (relativePath: %s)\n",
                //         entry.absoluteFilePath().toStdString().c_str(),
                //         relativePath.toStdString().c_str());

                // Считываем и десериализуем файл
                QFile sourceFile(entry.absoluteFilePath());
                if (sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QByteArray data = sourceFile.readAll();
                    sourceFile.close();

                    std::string content = data.toStdString();

                    try {
                        std::expected<Section, std::string> result = Json::deserialize<Section>(content);

                        if (result.has_value()) {
                            Section section = result.value();

                            // Проверяем наличие существующего файла в результирующей директории
                            QString   destPath = QDir(resultDir).filePath(relativePath);
                            QFileInfo destFileInfo(destPath);

                            // Создаем необходимые директории
                            QDir().mkpath(destFileInfo.absolutePath());

                            if (destFileInfo.exists()) {
                                // Если файл уже существует, объединяем транзакции
                                QFile destFile(destPath);
                                if (destFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                                    QByteArray destData = destFile.readAll();
                                    destFile.close();

                                    std::string                         destContent = destData.toStdString();
                                    std::expected<Section, std::string> destResult =
                                        Json::deserialize<Section>(destContent);

                                    if (destResult.has_value()) {
                                        Section destSection = destResult.value();

                                        // Объединяем транзакции
                                        size_t beforeSize = destSection.transactions.size();
                                        for (const auto& transaction : section.transactions) {
                                            destSection.transactions.insert(transaction);
                                        }
                                        size_t afterSize = destSection.transactions.size();

                                        // Сохраняем объединенный файл
                                        if (saveSection(destSection, destPath)) {
                                            // fprintf(stderr,
                                            //         "Updated existing file: %s (added %zu transactions)\n",
                                            //         destPath.toStdString().c_str(),
                                            //         afterSize - beforeSize);
                                        }
                                    } else {
                                        fprintf(stderr,
                                                "Failed to deserialize existing file: %s\n",
                                                destPath.toStdString().c_str());
                                    }
                                } else {
                                    fprintf(stderr,
                                            "Failed to open existing file: %s\n",
                                            destPath.toStdString().c_str());
                                }
                            } else {
                                // Если файла еще нет, просто сохраняем новый
                                if (saveSection(section, destPath)) {
                                    fprintf(stderr,
                                            "Saved new file: %s (transactions: %zu)\n",
                                            destPath.toStdString().c_str(),
                                            section.transactions.size());
                                }
                            }
                        } else {
                            fprintf(stderr,
                                    "Failed to deserialize source file: %s\n",
                                    entry.absoluteFilePath().toStdString().c_str());
                        }
                    } catch (const std::exception& e) {
                        fprintf(stderr, "Exception during processing: %s\n", e.what());
                    }
                } else {
                    fprintf(stderr,
                            "Failed to open source file: %s\n",
                            entry.absoluteFilePath().toStdString().c_str());
                }
            }
        }
    }
}

// Функция для объединения секций из разных архивов
void mergeDagFiles(const QString& archivesDir, const QString& resultDir) {
    // Настройка файла логов
    QFile logFile("dag_merger_debug.log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        fprintf(stderr, "Log file opened successfully\n");
    } else {
        fprintf(stderr, "Failed to open log file: %s\n", logFile.errorString().toStdString().c_str());
    }

    fprintf(stderr,
            "mergeDagFiles started. Archives dir: %s, Result dir: %s\n",
            archivesDir.toStdString().c_str(),
            resultDir.toStdString().c_str());

    // Создаем директорию для результатов, если ее нет
    QDir resultDirectory(resultDir);
    if (!resultDirectory.exists()) {
        bool created = resultDirectory.mkpath(".");
        fprintf(stderr, "Result directory created: %s\n", created ? "true" : "false");
    } else {
        fprintf(stderr, "Result directory already exists\n");
    }

    // Создаем временную директорию в указанном пути
    QString tempDirBase = archivesDir + "/tmp";
    QDir    tempDir(tempDirBase);
    if (!tempDir.exists()) {
        bool created = QDir().mkpath(tempDirBase);
        fprintf(stderr,
                "Temp directory created: %s - %s\n",
                tempDirBase.toStdString().c_str(),
                created ? "true" : "false");
    } else {
        fprintf(stderr, "Temp directory already exists\n");
    }

    // Получаем список архивов
    QDir        archivesDirectory(archivesDir);
    QStringList archives = archivesDirectory.entryList(QStringList() << "dag-*.tar.gz", QDir::Files);

    fprintf(stderr, "Found %d archives\n", archives.size());

    if (archives.isEmpty()) {
        fprintf(stderr, "No archives found in directory: %s\n", archivesDir.toStdString().c_str());
        logFile.close();
        return;
    }

    // Обрабатываем каждый архив
    for (const QString& archive : archives) {
        fprintf(stderr, "Processing archive: %s\n", archive.toStdString().c_str());

        // Создаем уникальную временную директорию для каждого архива
        QString currentTempDir = tempDirBase + "/" + QFileInfo(archive).baseName();
        QDir().mkpath(currentTempDir);

        // Распаковываем архив
        QString cmd =
            "tar -xzf \"" + archivesDirectory.absoluteFilePath(archive) + "\" -C \"" + currentTempDir + "\"";
        fprintf(stderr, "Executing command: %s\n", cmd.toStdString().c_str());

        int result = system(cmd.toStdString().c_str());
        fprintf(stderr, "Command result: %d\n", result);

        if (result != 0) {
            fprintf(stderr,
                    "Failed to extract archive: %s with error code: %d\n",
                    archive.toStdString().c_str(),
                    result);
            continue;
        }

        // Сразу сохраняем содержимое в директорию результатов
        fprintf(stderr, "Saving files directly to result directory...\n");

        // Обрабатываем каждый файл и сразу сохраняем результат
        processSaveDirectory(currentTempDir, resultDir);

        // Удаляем временную директорию
        QDir(currentTempDir).removeRecursively();
        fprintf(stderr, "Temporary directory removed\n");
    }

    fprintf(stderr, "All archives processed\n");
    logFile.close();
}
