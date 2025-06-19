#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <QCoreApplication>
#include <QDirIterator>
#include <string>
#include <expected>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <queue>
#include "chain/dag.h"

// Глобальные переменные для синхронизации
static std::unordered_map<std::string, std::unique_ptr<std::mutex>> g_fileMutexes;
static std::mutex g_fileMutexesMapMutex;
static std::atomic<int> g_processedFiles{0};
static std::atomic<int> g_totalFiles{0};

// Функция для получения относительного пути к файлу, начиная с 'dag/'
QString getRelativeDagPath(const QString& absolutePath, const QString& basePath) {
    int dagIndex = absolutePath.indexOf("/dag/");
    if (dagIndex != -1) {
        return absolutePath.mid(dagIndex + 1);
    }

    dagIndex = absolutePath.indexOf("dag/");
    if (dagIndex != -1) {
        return absolutePath.mid(dagIndex);
    }

    QString relativePath = QDir(basePath).relativeFilePath(absolutePath);
    if (relativePath.startsWith("dag/") || relativePath.indexOf("/dag/") != -1) {
        int idx = relativePath.indexOf("/dag/");
        if (idx != -1) {
            return relativePath.mid(idx + 1);
        }
        return relativePath;
    }

    return QString();
}

// Получение мьютекса для конкретного файла
std::mutex* getFileMutex(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(g_fileMutexesMapMutex);

    auto it = g_fileMutexes.find(filePath);
    if (it == g_fileMutexes.end()) {
        g_fileMutexes[filePath] = std::make_unique<std::mutex>();
        return g_fileMutexes[filePath].get();
    }
    return it->second.get();
}

// Вспомогательная функция для сохранения секции в файл
bool saveSection(const Section& section, const QString& filePath) {
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::string jsonData = Json::serialize(section);
        qint64 bytesWritten = file.write(jsonData.c_str(), jsonData.length());
        file.close();
        return bytesWritten > 0;
    }
    return false;
}

// Обработка одного файла
void processFile(const QString& filePath, const QString& sourceDir, const QString& resultDir) {
    QString relativePath = getRelativeDagPath(filePath, sourceDir);
    if (relativePath.isEmpty()) {
        return;
    }

           // Читаем файл
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        fprintf(stderr, "Failed to open file: %s\n", filePath.toStdString().c_str());
        return;
    }

    QByteArray data = file.readAll();
    file.close();

           // Десериализуем
    Section newSection;
    try {
        auto result = Json::deserialize<Section>(data.toStdString());
        if (!result.has_value()) {
            fprintf(stderr, "Failed to deserialize: %s\n", filePath.toStdString().c_str());
            return;
        }
        newSection = result.value();
    } catch (const std::exception& e) {
        fprintf(stderr, "Exception deserializing %s: %s\n", filePath.toStdString().c_str(), e.what());
        return;
    }

           // Определяем путь для сохранения
    QString destPath = QDir(resultDir).filePath(relativePath);
    QFileInfo destInfo(destPath);

           // Создаем директорию если нужно
    QDir().mkpath(destInfo.absolutePath());

           // Получаем мьютекс для этого файла
    std::mutex* fileMutex = getFileMutex(destPath.toStdString());
    std::lock_guard<std::mutex> lock(*fileMutex);

           // Читаем существующий файл если есть
    Section finalSection = newSection;

    if (destInfo.exists()) {
        QFile existingFile(destPath);
        if (existingFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray existingData = existingFile.readAll();
            existingFile.close();

            try {
                auto existingResult = Json::deserialize<Section>(existingData.toStdString());
                if (existingResult.has_value()) {
                    Section existingSection = existingResult.value();
                    // Объединяем транзакции
                    for (const auto& transaction : newSection.transactions) {
                        existingSection.transactions.insert(transaction);
                    }
                    finalSection = existingSection;
                }
            } catch (...) {
                // Используем новую секцию если не удалось прочитать существующую
            }
        }
    }

           // Сохраняем результат
    if (saveSection(finalSection, destPath)) {
        int processed = ++g_processedFiles;
        if (processed % 100 == 0) {
            fprintf(stderr, "Progress: %d/%d files (%.1f%%)\n",
                    processed, g_totalFiles.load(),
                    100.0 * processed / g_totalFiles.load());
        }
    } else {
        fprintf(stderr, "Failed to write: %s\n", destPath.toStdString().c_str());
    }
}

// Рабочий поток для обработки файлов
void fileWorker(std::queue<QString>& fileQueue, std::mutex& queueMutex,
                const QString& sourceDir, const QString& resultDir) {
    while (true) {
        QString filePath;

        // Получаем следующий файл из очереди
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (fileQueue.empty()) {
                break;
            }
            filePath = fileQueue.front();
            fileQueue.pop();
        }

        processFile(filePath, sourceDir, resultDir);
    }
}

// Обработка одного архива
void processArchive(const QString& archivePath, const QString& tempDirBase, const QString& resultDir) {
    QFileInfo archiveInfo(archivePath);
    QString archiveName = archiveInfo.fileName();

    fprintf(stderr, "\nProcessing archive: %s\n", archiveName.toStdString().c_str());

           // Создаем временную директорию для этого архива
    QString tempDir = tempDirBase + "/" + archiveInfo.baseName() + "_" +
                      QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(tempDir);

           // Распаковываем архив
    QString cmd = QString("tar -xzf \"%1\" -C \"%2\" 2>/dev/null").arg(archivePath).arg(tempDir);
    int result = system(cmd.toStdString().c_str());

    if (result != 0) {
        fprintf(stderr, "Failed to extract archive: %s\n", archiveName.toStdString().c_str());
        QDir(tempDir).removeRecursively();
        return;
    }

           // Собираем все файлы
    std::queue<QString> fileQueue;
    std::mutex queueMutex;

    QDirIterator it(tempDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        fileQueue.push(it.next());
    }

    int filesInArchive = fileQueue.size();
    g_totalFiles += filesInArchive;

    fprintf(stderr, "Found %d files in %s\n", filesInArchive, archiveName.toStdString().c_str());

           // Определяем количество потоков для обработки файлов
    int numThreads = std::min(8, std::max(1, filesInArchive / 50));

    // Запускаем потоки для обработки файлов
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(fileWorker, std::ref(fileQueue), std::ref(queueMutex),
                             tempDir, resultDir);
    }

           // Ждем завершения всех потоков
    for (auto& thread : threads) {
        thread.join();
    }

           // Удаляем временную директорию
    QDir(tempDir).removeRecursively();

    fprintf(stderr, "Finished processing archive: %s\n", archiveName.toStdString().c_str());
}

// Основная функция
void mergeDagFiles(const QString& archivesDir, const QString& resultDir) {
    fprintf(stderr, "=== DAG Merge Started ===\n");
    fprintf(stderr, "Archives directory: %s\n", archivesDir.toStdString().c_str());
    fprintf(stderr, "Result directory: %s\n", resultDir.toStdString().c_str());

           // Создаем директорию для результатов
    QDir().mkpath(resultDir);

           // Создаем временную директорию
    QString tempDirBase = archivesDir + "/tmp_merge";
    QDir().mkpath(tempDirBase);

           // Получаем список архивов
    QDir dir(archivesDir);
    QStringList archives = dir.entryList(QStringList() << "dag-*.tar.gz", QDir::Files);

    fprintf(stderr, "Found %d archives to process\n", archives.size());

    if (archives.isEmpty()) {
        fprintf(stderr, "No archives found!\n");
        return;
    }

           // Сбрасываем счетчики
    g_processedFiles = 0;
    g_totalFiles = 0;

           // Преобразуем в полные пути
    std::vector<QString> archivePaths;
    for (const QString& archive : archives) {
        archivePaths.push_back(dir.absoluteFilePath(archive));
    }

           // Определяем количество потоков для архивов
    int numArchiveThreads = std::min(4, (int)archives.size());
    fprintf(stderr, "Using %d threads for archive processing\n", numArchiveThreads);

           // Создаем очередь архивов
    std::queue<QString> archiveQueue;
    std::mutex archiveQueueMutex;
    for (const auto& path : archivePaths) {
        archiveQueue.push(path);
    }

           // Запускаем потоки для обработки архивов
    std::vector<std::thread> archiveThreads;
    for (int i = 0; i < numArchiveThreads; ++i) {
        archiveThreads.emplace_back([&]() {
            while (true) {
                QString archivePath;

                // Получаем следующий архив
                {
                    std::lock_guard<std::mutex> lock(archiveQueueMutex);
                    if (archiveQueue.empty()) {
                        break;
                    }
                    archivePath = archiveQueue.front();
                    archiveQueue.pop();
                }

                processArchive(archivePath, tempDirBase, resultDir);
            }
        });
    }

           // Ждем завершения всех потоков
    for (auto& thread : archiveThreads) {
        thread.join();
    }

           // Очистка
    QDir(tempDirBase).removeRecursively();
    g_fileMutexes.clear();

    fprintf(stderr, "\n=== DAG Merge Completed ===\n");
    fprintf(stderr, "Total processed files: %d\n", g_processedFiles.load());
}

// Упрощенная версия для последовательной обработки архивов
// (если нужно экономить память)
void mergeDagFilesSequential(const QString& archivesDir, const QString& resultDir) {
    fprintf(stderr, "=== DAG Merge Started (Sequential Mode) ===\n");

           // Создаем директории
    QDir().mkpath(resultDir);
    QString tempDirBase = archivesDir + "/tmp_merge";
    QDir().mkpath(tempDirBase);

           // Получаем список архивов
    QDir dir(archivesDir);
    QStringList archives = dir.entryList(QStringList() << "dag-*.tar.gz", QDir::Files);

    fprintf(stderr, "Found %d archives to process\n", archives.size());

    if (archives.isEmpty()) {
        return;
    }

           // Сбрасываем счетчики
    g_processedFiles = 0;
    g_totalFiles = 0;

           // Обрабатываем архивы последовательно
    int archiveNum = 0;
    for (const QString& archive : archives) {
        archiveNum++;
        fprintf(stderr, "\n[%d/%d] ", archiveNum, archives.size());

        QString archivePath = dir.absoluteFilePath(archive);
        processArchive(archivePath, tempDirBase, resultDir);
    }

           // Очистка
    QDir(tempDirBase).removeRecursively();
    g_fileMutexes.clear();

    fprintf(stderr, "\n=== DAG Merge Completed ===\n");
    fprintf(stderr, "Total processed files: %d\n", g_processedFiles.load());
}
