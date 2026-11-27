#include "ExtractTool.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QUuid>
#include <archive.h>
#include <archive_entry.h>

namespace {

    constexpr size_t kReadBlockBytes = 256 * 1024;

    // 条目路径必须是相对路径且不含 ".."：不信任压缩包，防止写出目标目录之外（zip-slip）
    bool isSafeEntryPath(const QString& path)
    {
        if (path.isEmpty()) return false;
        if (path.startsWith('/') || path.startsWith('\\')) return false;
        if (path.size() >= 2 && path.at(1) == ':') return false;
        const QStringList parts = QString(path).replace('\\', '/').split('/');
        return !parts.contains(QStringLiteral(".."));
    }

    QString entryPath(archive_entry* e)
    {
        const char* utf8 = archive_entry_pathname_utf8(e);
        return utf8 ? QString::fromUtf8(utf8) : QString::fromLocal8Bit(archive_entry_pathname(e));
    }

    // 目标路径若经过符号链接（如 macOS 的 /var -> /private/var），SECURE_SYMLINKS 会拒绝写入；先解析成真实路径，包内的符号链接仍被拦截。
    QString resolvedDestDir(const QString& destDir)
    {
        const QString native = QDir(QDir::fromNativeSeparators(destDir)).absolutePath();
        const QString canonical = QFileInfo(native).canonicalFilePath();
        if (!canonical.isEmpty()) return canonical;

        QStringList tail;
        QDir cur(native);
        while (!cur.exists() && cur.path() != cur.rootPath() && !cur.path().isEmpty()) {
            tail.prepend(cur.dirName());
            cur.cdUp();
        }
        QString base = QFileInfo(cur.path()).canonicalFilePath();
        if (base.isEmpty()) base = cur.path();
        for (const QString& part : tail) base += '/' + part;
        return base;
    }

    // 进程内解压，不调用系统 tar：Windows 的 bsdtar 没有 bzip2、干净机器上会失败；三平台统一 libarchive。阻塞，须放工作线程。
    bool runExtract(const QString& archivePath, const QString& destDir,
        const ExtractProgressCb& onProgress, QString* error)
    {
        auto fail = [&](const QString& why) {
            LOG_WARN(QString("Extract failed: %1 (%2)").arg(why, archivePath));
            if (error) *error = why;
            return false;
        };

        archive* in = archive_read_new();
        archive* out = archive_write_disk_new();
        struct Guard {
            archive* in; archive* out;
            ~Guard() { archive_read_free(in); archive_write_free(out); }
        } guard{ in, out };

        archive_read_support_format_tar(in);
        archive_read_support_format_zip(in);
        archive_read_support_filter_bzip2(in);
        archive_read_support_filter_gzip(in);

        // PERM：mac/linux 上还原可执行位；SECURE_*：拒绝 ".." 与经符号链接逃出目标目录
        archive_write_disk_set_options(out,
            ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
            ARCHIVE_EXTRACT_SECURE_NODOTDOT | ARCHIVE_EXTRACT_SECURE_SYMLINKS);

#ifdef Q_OS_WIN
        // 宽字符版本才能打开含中文用户名的路径
        const std::wstring wpath = QDir::toNativeSeparators(archivePath).toStdWString();
        int r = archive_read_open_filename_w(in, wpath.c_str(), kReadBlockBytes);
#else
        int r = archive_read_open_filename(in, QFile::encodeName(archivePath).constData(), kReadBlockBytes);
#endif
        if (r != ARCHIVE_OK) return fail(QString::fromUtf8(archive_error_string(in)));

        const qint64 fileSize = qMax<qint64>(1, QFileInfo(archivePath).size());
        const QString base = resolvedDestDir(destDir);
        int lastPermille = -1;
        QString lastFile;

        // 总条目数要先通读一遍才知道，对 GB 级压缩包代价太大；按"压缩包已读字节 / 文件大小"折算千分比
        auto report = [&](const QString& file) {
            if (!onProgress) return;
            const int permille = int(qMin<qint64>(1000, archive_filter_bytes(in, -1) * 1000 / fileSize));
            if (permille == lastPermille && file == lastFile) return;
            lastPermille = permille;
            lastFile = file;
            ExtractProgress p;
            p.current = permille;
            p.total = 1000;
            p.currentFile = file;
            onProgress(p);
        };
        report(QString());

        archive_entry* entry = nullptr;
        while ((r = archive_read_next_header(in, &entry)) != ARCHIVE_EOF) {
            if (r < ARCHIVE_WARN) return fail(QString::fromUtf8(archive_error_string(in)));

            const QString rel = entryPath(entry);
            if (!isSafeEntryPath(rel)) return fail(QStringLiteral("unsafe entry path: ") + rel);
            archive_entry_update_pathname_utf8(entry, (base + '/' + rel).toUtf8().constData());

            if (const char* link = archive_entry_hardlink_utf8(entry)) {
                const QString linkRel = QString::fromUtf8(link);
                if (!isSafeEntryPath(linkRel)) return fail(QStringLiteral("unsafe hardlink: ") + linkRel);
                archive_entry_update_hardlink_utf8(entry, (base + '/' + linkRel).toUtf8().constData());
            }

            if (archive_write_header(out, entry) < ARCHIVE_WARN)
                return fail(QString::fromUtf8(archive_error_string(out)));

            const QString fileName = rel.mid(rel.lastIndexOf('/') + 1);
            if (archive_entry_filetype(entry) == AE_IFREG) {
                const void* buf = nullptr;
                size_t size = 0;
                la_int64_t offset = 0;
                while ((r = archive_read_data_block(in, &buf, &size, &offset)) == ARCHIVE_OK) {
                    if (archive_write_data_block(out, buf, size, offset) < ARCHIVE_WARN)
                        return fail(QString::fromUtf8(archive_error_string(out)));
                    report(fileName);
                }
                if (r != ARCHIVE_EOF) return fail(QString::fromUtf8(archive_error_string(in)));
            }
            archive_write_finish_entry(out);
            report(fileName);
        }
        report(QString());
        return true;
    }

}


bool ExtractTool::flattenWrapper(const QString& dir, const QString& wrapperName)
{
    QDir root(dir);
    // 以 "." 开头的是安装标记（.installing）：它和包裹目录同时存在，不能算作"目录里还有别的东西"，
    // 否则单层包裹目录不会被剥掉，模型文件留在 X/X/ 下，sherpa 找不到文件、识别器悄悄失效、转录恒为空
    QFileInfoList top;
    for (const QFileInfo& e : root.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden))
        if (!e.fileName().startsWith('.')) top << e;
    if (top.size() != 1 || !top.first().isDir()) return false;
    if (!wrapperName.isEmpty() && top.first().fileName() != wrapperName) return false;

    // 先把包裹目录改成临时名：里面可能有和它同名的子项，直接上移会冲突
    const QString tmpPath = root.filePath(QStringLiteral(".flatten_") + QUuid::createUuid().toString(QUuid::Id128));
    if (!QDir().rename(top.first().absoluteFilePath(), tmpPath)) {
        LOG_WARN("Cannot flatten wrapper directory (rename failed): " + top.first().absoluteFilePath());
        return false;
    }
    QDir wrapper(tmpPath);
    bool allMoved = true;
    for (const QFileInfo& c : wrapper.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden)) {
        if (!QDir().rename(c.absoluteFilePath(), root.filePath(c.fileName()))) {
            LOG_WARN("Cannot move out of wrapper directory: " + c.absoluteFilePath());
            allMoved = false;
        }
    }
    if (allMoved) wrapper.removeRecursively();
    else QDir().rename(tmpPath, top.first().absoluteFilePath());
    return allMoved;
}

bool ExtractTool::extractAll(const QString& archivePath,
    const QString& destinationDir,
    bool removeArchiveAfterExtract,
    const ExtractProgressCb& onProgress,
    QString* error)
{
    if (!QFileInfo::exists(archivePath) || destinationDir.isEmpty()) {
        LOG_WARN("Bad arguments for extractAll.");
        if (error) *error = QStringLiteral("bad arguments");
        return false;
    }

    const bool destPreExisting = QDir(destinationDir).exists();
    if (!QDir().mkpath(destinationDir)) {
        LOG_WARN("Failed to create destination dir: " + destinationDir);
        if (error) *error = QStringLiteral("cannot create directory: ") + destinationDir;
        return false;
    }

    if (!runExtract(archivePath, destinationDir, onProgress, error)) {
        if (!destPreExisting) QDir(destinationDir).removeRecursively();
        return false;
    }

    flattenWrapper(destinationDir);

    if (removeArchiveAfterExtract) QFile::remove(archivePath);
    LOG_DEBUG(QString("Extracted %1 -> %2").arg(archivePath, destinationDir));
    return true;
}


bool ExtractTool::extractAndDeploy(const QString& archivePath, const ExtractOptions& options, QString* error)
{
    if (!QFileInfo::exists(archivePath) || options.destinationDir.isEmpty()) {
        LOG_WARN("Bad arguments for extractAndDeploy.");
        if (error) *error = QStringLiteral("bad arguments");
        return false;
    }

    const QString tempDir = QFileInfo(options.destinationDir).absolutePath()
        + "/.extract_" + QUuid::createUuid().toString(QUuid::Id128);
    if (!QDir().mkpath(tempDir)) {
        LOG_WARN("Failed to create temp dir: " + tempDir);
        if (error) *error = QStringLiteral("cannot create directory: ") + tempDir;
        return false;
    }

    if (!runExtract(archivePath, tempDir, options.onProgress, error)) {
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
        if (QFile::exists(dst) && !QFile::remove(dst)) {
            // Windows 上已被加载的 DLL 删不掉但能改名：挪开后放新文件，重启即生效；
            // 否则新文件拷不进去，GPU 运行库会一直停留在旧的 CPU 版
            const QString stale = dst + ".old";
            QFile::remove(stale);
            if (!QFile::rename(dst, stale)) LOG_WARN("File in use and cannot be moved aside: " + dst);
        }

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
    if (processed == 0 && error) *error = QStringLiteral("no matching files in archive");
    return processed > 0;
}
