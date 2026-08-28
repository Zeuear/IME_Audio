#include "ExtractTool.h"
#include <QDir>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QUuid>

namespace {

    // 从 tar -xvf 的一行输出中取末级文件名；目录条目返回空
    QString parseTarEntry(const QByteArray& rawLine)
    {
        QString line = QString::fromUtf8(rawLine).trimmed();
        if (line.isEmpty() || line.endsWith('/')) return {};
        if (line.startsWith("./")) line = line.mid(2);
        const int slash = line.lastIndexOf('/');
        return (slash >= 0) ? line.mid(slash + 1) : line;
    }

    // 解压到指定目录。阻塞直到完成，调用方应放在工作线程里。
    bool runTar(const QString& archivePath, const QString& destDir,
        const std::function<void(const QString&)>& onEntry)
    {
        QProcess tar;
        tar.setProgram("tar");
        tar.setArguments({ "-xvf", archivePath, "-C", destDir });
        tar.setProcessChannelMode(QProcess::MergedChannels);  
        tar.start();

        if (!tar.waitForStarted(5000)) {
            LOG_WARN("Failed to start tar.");
            return false;
        }

        QByteArray tail;
        QStringList recentLines;

        auto consume = [&](const QByteArray& raw) {
            const QString line = QString::fromUtf8(raw).trimmed();
            if (line.isEmpty()) return;
            recentLines.append(line);
            if (recentLines.size() > 20) recentLines.removeFirst();
            const QString name = parseTarEntry(raw);
            if (!name.isEmpty() && onEntry) onEntry(name);
        };

        while (tar.state() == QProcess::Running) {
            if (!tar.waitForReadyRead(-1)) break;
            tail += tar.readAll();
            int idx;
            while ((idx = tail.indexOf('\n')) >= 0) {
                consume(tail.left(idx));
                tail.remove(0, idx + 1);
            }
        }
        tar.waitForFinished(-1);

        tail += tar.readAll(); 
        for (const QByteArray& raw : tail.split('\n')) consume(raw);

        if (tar.exitStatus() != QProcess::NormalExit || tar.exitCode() != 0) {
            LOG_WARN(QString("tar failed, exit=%1:\n%2")
                .arg(tar.exitCode()).arg(recentLines.join('\n')));
            return false;
        }
        return true;
    }

} // namespace


bool ExtractTool::extractAll(const QString& archivePath,
    const QString& destinationDir,
    bool removeArchiveAfterExtract,
    const ExtractProgressCb& onProgress,
    int /*stallTimeoutMs*/)          
{
    if (!QFileInfo::exists(archivePath) || destinationDir.isEmpty()) {
        LOG_WARN("Bad arguments for extractAll.");
        return false;
    }

    const bool destPreExisting = QDir(destinationDir).exists();
    if (!QDir().mkpath(destinationDir)) {
        LOG_WARN("Failed to create destination dir: " + destinationDir);
        return false;
    }

    ExtractProgress prog;
    prog.total = -1;                                         
    int count = 0;

    const bool ok = runTar(archivePath, destinationDir, [&](const QString& name) {
        if (!onProgress) return;
        prog.current = ++count;
        prog.currentFile = name;
        onProgress(prog);
        });

    if (!ok) {
        if (!destPreExisting) QDir(destinationDir).removeRecursively();
        return false;
    }

    // 剥掉单层包裹目录
    QDir root(destinationDir);
    const QFileInfoList top = root.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    if (top.size() == 1 && top.first().isDir()) {
        QDir wrapper(top.first().absoluteFilePath());
        for (const QFileInfo& c : wrapper.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
            QDir().rename(c.absoluteFilePath(), root.filePath(c.fileName()));
        }
        wrapper.removeRecursively();
    }

    if (removeArchiveAfterExtract) QFile::remove(archivePath);
    LOG_DEBUG(QString("Extracted %1 -> %2").arg(archivePath, destinationDir));
    return true;
}


bool ExtractTool::extractAndDeploy(const QString& archivePath, const ExtractOptions& options)
{
    if (!QFileInfo::exists(archivePath) || options.destinationDir.isEmpty()) {
        LOG_WARN("Bad arguments for extractAndDeploy.");
        return false;
    }

    // 临时目录放目标盘旁边
    const QString tempDir = QFileInfo(options.destinationDir).absolutePath()
        + "/.extract_" + QUuid::createUuid().toString(QUuid::Id128);
    if (!QDir().mkpath(tempDir)) {
        LOG_WARN("Failed to create temp dir: " + tempDir);
        return false;
    }

    ExtractProgress prog;
    prog.total = -1;
    int count = 0;

    const bool ok = runTar(archivePath, tempDir, [&](const QString& name) {
        if (!options.onProgress) return;
        prog.current = ++count;
        prog.currentFile = name;
        options.onProgress(prog);
        });

    if (!ok) {
        QDir(tempDir).removeRecursively();
        return false;
    }

    QDir().mkpath(options.destinationDir);
    int processed = 0;
    QDirIterator it(tempDir, options.nameFilters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo fi(it.next());
        if (options.filter && !options.filter(fi)) continue;

        const QString dst = QDir(options.destinationDir).filePath(fi.fileName());
        QFile::remove(dst);

        bool done = options.moveInsteadOfCopy
            ? QFile::rename(fi.absoluteFilePath(), dst)
            : QFile::copy(fi.absoluteFilePath(), dst);
        if (!done && options.moveInsteadOfCopy) {           
            done = QFile::copy(fi.absoluteFilePath(), dst);
        }

        if (done) ++processed;
        else LOG_WARN("Failed to deploy: " + fi.fileName());
    }

    QDir(tempDir).removeRecursively();
    if (options.removeArchiveAfterExtract) QFile::remove(archivePath);
    LOG_DEBUG(QString("Deployed %1 files to %2").arg(processed).arg(options.destinationDir));
    return processed > 0;
}