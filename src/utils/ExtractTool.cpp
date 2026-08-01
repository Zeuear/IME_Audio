#include "ExtractTool.h"
#include <QDir>
#include <QProcess>
#include <QApplication>
#include <QDebug>
#include <QUuid>
#include <QDirIterator>
#include <QFile>
#include <QTimer>


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
        LOG_WARN("Failed to create temp dir:"+ tempExtractDir);
        return false;
    }

    QProcess tarProcess;
    tarProcess.setProgram("tar");
    tarProcess.setArguments({ "-xvf", archivePath, "-C", tempExtractDir });

    QEventLoop loop;
    bool timedOutByStall = false;
    bool processFinished = false;

    QTimer stallTimer;
    stallTimer.setSingleShot(true);
    const int stallTimeoutMs = options.stallTimeoutMs > 0 ? options.stallTimeoutMs : 90000;

    auto resetStallTimer = [&]() {
        stallTimer.start(stallTimeoutMs);
        };

    QObject::connect(&stallTimer, &QTimer::timeout, [&]() {
        qDebug() << "No output for" << stallTimeoutMs << "ms, treating as stalled. Killing tar.";
        timedOutByStall = true;
        tarProcess.kill();
        });

    auto handleOutput = [&]() {
        resetStallTimer();
        const QByteArray out = tarProcess.readAllStandardOutput();
        const QByteArray err = tarProcess.readAllStandardError();
        if (options.onProgressLine) {
            for (const QByteArray& line : out.split('\n')) {
                if (!line.trimmed().isEmpty()) options.onProgressLine(QString::fromUtf8(line));
            }
            for (const QByteArray& line : err.split('\n')) {
                if (!line.trimmed().isEmpty()) options.onProgressLine(QString::fromUtf8(line));
            }
        }
        };

    QObject::connect(&tarProcess, &QProcess::readyReadStandardOutput, handleOutput);
    QObject::connect(&tarProcess, &QProcess::readyReadStandardError, handleOutput);

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
        QDir(tempExtractDir).removeRecursively();
        QDir(options.destinationDir).removeRecursively();
        return false;
    }

    resetStallTimer();
    loop.exec();        

    if (timedOutByStall) {
        LOG_WARN("Extraction aborted: process stalled with no output.");
        QDir(tempExtractDir).removeRecursively();
        QDir(options.destinationDir).removeRecursively();
        return false;
    }

    if (!processFinished || tarProcess.exitCode() != 0) {
        LOG_WARN(QString("Extraction failed. exitCode:%1 stderr: %2")
            .arg(tarProcess.exitCode()).arg(tarProcess.readAllStandardError()));
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

        bool ok = false;
        if (options.moveInsteadOfCopy) {
            ok = QFile::rename(filePath, destination);
            if (!ok) {
                ok = QFile::copy(filePath, destination) && QFile::remove(filePath);
            }
        }
        else {
            ok = QFile::copy(filePath, destination);
        }

        if (ok) {
            LOG_DEBUG(QString("Deployed:%1 -> %1").arg(extractedFileInfo.fileName()).arg(destination));
            processedCount++;
        }
        else {
            LOG_WARN(QString("Failed to deploy:").arg(extractedFileInfo.fileName()));
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
                            const std::function<void(const QString&)>& onProgressLine,
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

    QProcess tarProcess;
    tarProcess.setProgram("tar");
    tarProcess.setArguments({ "-xvf", archivePath, "-C", tempExtractDir });

    QEventLoop loop;
    bool timedOutByStall = false;
    bool processFinished = false;

    QTimer stallTimer;
    stallTimer.setSingleShot(true);
    const int effectiveStallTimeout = stallTimeoutMs > 0 ? stallTimeoutMs : 900000;

    auto resetStallTimer = [&]() {
        stallTimer.start(effectiveStallTimeout);
        };

    QObject::connect(&stallTimer, &QTimer::timeout, [&]() {
        LOG_WARN(QString("No output for %1 ms, treating as stalled. Killing tar.").arg(effectiveStallTimeout));
        timedOutByStall = true;
        tarProcess.kill();
        });

    auto handleOutput = [&]() {
        resetStallTimer();
        const QByteArray out = tarProcess.readAllStandardOutput();
        const QByteArray err = tarProcess.readAllStandardError();
        if (onProgressLine) {
            for (const QByteArray& line : out.split('\n')) {
                if (!line.trimmed().isEmpty()) onProgressLine(QString::fromUtf8(line));
            }
            for (const QByteArray& line : err.split('\n')) {
                if (!line.trimmed().isEmpty()) onProgressLine(QString::fromUtf8(line));
            }
        }
        };

    QObject::connect(&tarProcess, &QProcess::readyReadStandardOutput, handleOutput);
    QObject::connect(&tarProcess, &QProcess::readyReadStandardError, handleOutput);

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
        QDir(tempExtractDir).removeRecursively();
        return false;
    }

    resetStallTimer();
    loop.exec();

    if (timedOutByStall) {
        LOG_WARN("Extraction aborted: process stalled with no output.");
        QDir(tempExtractDir).removeRecursively();
        QDir(destinationDir).removeRecursively();
        return false;
    }

    if (!processFinished || tarProcess.exitCode() != 0) {
        LOG_WARN(QString("Extraction failed. exitCode:%1 stderr: %2")
            .arg(tarProcess.exitCode()).arg(QString::fromUtf8(tarProcess.readAllStandardError())));
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
            }
            else {
                if (QFile::exists(targetPath)) {
                    QFile::remove(targetPath);
                }
                if (QFile::copy(entry.absoluteFilePath(), targetPath)) {
                    LOG_DEBUG(QString("Deployed: %1").arg(targetPath));
                }
                else {
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