#include "ExtractTool.h"
#include <QDir>
#include <QProcess>
#include <QApplication>
#include <QDebug>
#include <QUuid>
#include <QDirIterator>
#include <QFile>
#include <QTimer>
#include <QRegularExpression>


namespace {

// 解析 tar 输出的一行，提取末级文件名（去掉前导 ./ 与目录路径）
QString parseTarEntry(const QByteArray& rawLine)
{
    QString line = QString::fromUtf8(rawLine).trimmed();
    if (line.isEmpty()) return {};
    // tar -tf 输出形如: "./foo/bar.onnx" 或 "foo/bar.onnx" 或带尾斜杠的目录
    if (line.endsWith('/')) return {}; // 跳过目录条目
    int slash = line.lastIndexOf('/');
    QString name = (slash >= 0) ? line.mid(slash + 1) : line;
    if (name.startsWith("./")) name = name.mid(2);
    return name;
}

// 先列目录得到压缩包内条目总数。失败返回 -1（降级：未知模式）。
int countArchiveEntries(const QString& archivePath)
{
    QFileInfo file(archivePath);
    QString suffix = file.suffix();
    if (suffix == "bz2") {
        return -1;
    }

    QProcess ls;
    ls.setProgram("tar");
    ls.setArguments({ "-tf", archivePath });
    ls.start();
    if (!ls.waitForFinished(30000)) {
        LOG_WARN(QString("tar -tf failed/timeout for %1, falling back to unknown total").arg(archivePath));
        return -1;
    }
    if (ls.exitCode() != 0) {
        LOG_WARN(QString("tar -tf exit=%1 for %2, falling back to unknown total")
                     .arg(ls.exitCode()).arg(archivePath));
        return -1;
    }
    const QByteArray out = ls.readAllStandardOutput();
    int count = 0;
    for (const QByteArray& line : out.split('\n')) {
        if (!parseTarEntry(line).isEmpty()) ++count;
    }
    return count > 0 ? count : -1;
}

// 运行 tar -xvf 解压，逐条回报进度；返回是否成功启动并完成。
bool runTarExtract(const QString& archivePath, const QString& extractDir,
                   const std::function<void(const QString&)>& onEntry,
                   int stallTimeoutMs)
{
    QProcess tarProcess;
    tarProcess.setProgram("tar");
    tarProcess.setArguments({ "-xvf", archivePath, "-C", extractDir });

    QEventLoop loop;
    bool timedOutByStall = false;
    bool processFinished = false;

    QTimer stallTimer;
    stallTimer.setSingleShot(true);
    const int effectiveStallTimeout = stallTimeoutMs > 0 ? stallTimeoutMs : 900000;

    auto resetStallTimer = [&]() { stallTimer.start(effectiveStallTimeout); };

    QObject::connect(&stallTimer, &QTimer::timeout, [&]() {
        LOG_WARN(QString("No output for %1 ms, treating as stalled. Killing tar.").arg(effectiveStallTimeout));
        timedOutByStall = true;
        tarProcess.kill();
    });

    QObject::connect(&tarProcess, &QProcess::readyReadStandardOutput, [&]() {
        resetStallTimer();
        const QByteArray out = tarProcess.readAllStandardOutput();
        for (const QByteArray& line : out.split('\n')) {
            QString name = parseTarEntry(line);
            if (!name.isEmpty() && onEntry) onEntry(name);
        }
    });
    QObject::connect(&tarProcess, &QProcess::readyReadStandardError, [&]() {
        resetStallTimer();
        const QByteArray err = tarProcess.readAllStandardError();
        for (const QByteArray& line : err.split('\n')) {
            QString name = parseTarEntry(line);
            if (!name.isEmpty() && onEntry) onEntry(name);
        }
    });

    QObject::connect(&tarProcess, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
        [&](int, QProcess::ExitStatus) {
            processFinished = true;
            stallTimer.stop();
            loop.quit();
        });
    QObject::connect(&tarProcess, &QProcess::errorOccurred, [&](QProcess::ProcessError) {
        stallTimer.stop();
        loop.quit();
    });

    tarProcess.start();
    if (!tarProcess.waitForStarted(5000)) {
        LOG_WARN("Failed to start tar process.");
        return false;
    }

    resetStallTimer();
    loop.exec();

    if (timedOutByStall) {
        LOG_WARN("Extraction aborted: process stalled with no output.");
        return false;
    }
    if (!processFinished || tarProcess.exitCode() != 0) {
        LOG_WARN(QString("Extraction failed. exitCode:%1 stderr: %2")
                     .arg(tarProcess.exitCode()).arg(QString::fromUtf8(tarProcess.readAllStandardError())));
        return false;
    }
    return true;
}

} // namespace


bool ExtractTool::extractAndDeploy(const QString& archivePath, const ExtractOptions& options)
{
    QFileInfo fileInfo(archivePath);
    if (!fileInfo.exists()) {
        LOG_WARN("Archive does not exist:" + archivePath);
        return false;
    }

    const QString tempExtractDir = QDir::tempPath() + "/sherpa_extract_"
        + QUuid::createUuid().toString(QUuid::Id128);
    if (!QDir().mkpath(tempExtractDir)) {
        LOG_WARN("Failed to create temp dir:" + tempExtractDir);
        return false;
    }

    const int total = options.onProgress ? countArchiveEntries(archivePath) : -1;
    ExtractProgress prog;
    prog.total = total;
    int current = 0;

    bool ok = runTarExtract(archivePath, tempExtractDir,
        [&](const QString& name) {
            if (options.onProgress) {
                prog.current = ++current;
                prog.currentFile = name;
                options.onProgress(prog);
            }
        }, options.stallTimeoutMs);

    if (!ok) {
        QDir(tempExtractDir).removeRecursively();
        QDir(options.destinationDir).removeRecursively();
        return false;
    }

    QDir().mkpath(options.destinationDir);
    QDirIterator it(tempExtractDir, options.nameFilters, QDir::Files, QDirIterator::Subdirectories);

    int processedCount = 0;
    while (it.hasNext()) {
        const QString filePath = it.next();
        const QFileInfo extractedFileInfo(filePath);

        if (options.filter && !options.filter(extractedFileInfo)) {
            continue;
        }

        QString destination = QDir(options.destinationDir).filePath(extractedFileInfo.fileName());
        if (QFile::exists(destination)) {
            QFile::remove(destination);
        }

        bool deployed = false;
        if (options.moveInsteadOfCopy) {
            deployed = QFile::rename(filePath, destination);
            if (!deployed) {
                deployed = QFile::copy(filePath, destination) && QFile::remove(filePath);
            }
        } else {
            deployed = QFile::copy(filePath, destination);
        }

        if (deployed) {
            LOG_DEBUG(QString("Deployed: %1 -> %2").arg(extractedFileInfo.fileName()).arg(destination));
            processedCount++;
        } else {
            LOG_WARN(QString("Failed to deploy: %1").arg(extractedFileInfo.fileName()));
        }
    }

    QDir(tempExtractDir).removeRecursively();
    if (options.removeArchiveAfterExtract) {
        QFile::remove(archivePath);
    }
    LOG_DEBUG(QString("Extraction complete. Processed %1 files to %2")
                  .arg(processedCount).arg(options.destinationDir));
    return processedCount > 0;
}


bool ExtractTool::extractAll(const QString& archivePath,
                            const QString& destinationDir,
                            bool removeArchiveAfterExtract,
                            const ExtractProgressCb& onProgress,
                            int stallTimeoutMs)
{
    QFileInfo fileInfo(archivePath);
    if (!fileInfo.exists()) {
        LOG_WARN("Archive does not exist: " + archivePath);
        return false;
    }

    const QString tempExtractDir = QDir::tempPath() + "/simple_extract_"
        + QUuid::createUuid().toString(QUuid::Id128);
    if (!QDir().mkpath(tempExtractDir)) {
        LOG_WARN("Failed to create temp dir: " + tempExtractDir);
        return false;
    }

    const int total = onProgress ? countArchiveEntries(archivePath) : -1;
    ExtractProgress prog;
    prog.total = total;
    int current = 0;

    bool ok = runTarExtract(archivePath, tempExtractDir,
        [&](const QString& name) {
            if (onProgress) {
                prog.current = ++current;
                prog.currentFile = name;
                onProgress(prog);
            }
        }, stallTimeoutMs);

    if (!ok) {
        QDir(tempExtractDir).removeRecursively();
        QDir(destinationDir).removeRecursively();
        return false;
    }

    QString sourceRoot = tempExtractDir;
    QDir topDir(tempExtractDir);
    QFileInfoList topEntries = topDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);

    if (topEntries.size() == 1 && topEntries.first().isDir()) {
        sourceRoot = topEntries.first().absoluteFilePath();
        LOG_DEBUG(QString("Detected single wrapper directory '%1', stripping this level.")
                      .arg(topEntries.first().fileName()));
    }

    if (!QDir().mkpath(destinationDir)) {
        LOG_WARN("Failed to create destination dir: " + destinationDir);
        QDir(tempExtractDir).removeRecursively();
        QDir(destinationDir).removeRecursively();
        return false;
    }

    std::function<bool(const QString&, const QString&)> copyRecursively =
        [&](const QString& srcPath, const QString& dstPath) -> bool {
        QDir srcDir(srcPath);
        if (!QDir().mkpath(dstPath)) {
            LOG_WARN("Failed to create dir: " + dstPath);
            return false;
        }

        bool allOk = true;
        const QFileInfoList entries = srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
        for (const QFileInfo& entry : entries) {
            const QString targetPath = QDir(dstPath).filePath(entry.fileName());

            if (entry.isDir()) {
                allOk = copyRecursively(entry.absoluteFilePath(), targetPath) && allOk;
            } else {
                if (QFile::exists(targetPath)) {
                    QFile::remove(targetPath);
                }
                if (QFile::copy(entry.absoluteFilePath(), targetPath)) {
                    LOG_DEBUG(QString("Deployed: %1").arg(targetPath));
                } else {
                    LOG_WARN(QString("Failed to deploy: %1").arg(entry.absoluteFilePath()));
                    allOk = false;
                }
            }
        }
        return allOk;
        };

    bool success = copyRecursively(sourceRoot, destinationDir);
    QDir(tempExtractDir).removeRecursively();
    if (removeArchiveAfterExtract) {
        QFile::remove(archivePath);
    }

    LOG_DEBUG(QString("Extraction complete: %1 -> %2, success=%3")
                  .arg(archivePath, destinationDir, success ? "true" : "false"));
    if (!success) {
        QDir(destinationDir).removeRecursively();
    }

    return success;
}
