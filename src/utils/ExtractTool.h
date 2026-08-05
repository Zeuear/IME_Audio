#pragma once
#include <functional>
#include <QApplication>
#include <QFileInfo>
#include "Logger.h"


struct ExtractProgress {
    int current = 0;       // 已解压条目数（从 0 递增）
    int total = -1;        // 压缩包内条目总数；-1 表示未知（tar -tf 失败时的降级）
    QString currentFile;   // 当前正在解压的文件名（仅末级，已去路径前缀）
};

using ExtractProgressCb = std::function<void(const ExtractProgress&)>;

struct ExtractOptions
{
    // 目标目录，默认使用程序根目录
    QString destinationDir = QApplication::applicationDirPath();

    // 需要匹配的文件后缀
    QStringList nameFilters = { "*.dll", "*.so*", "*.dylib" };

    // 返回 true 才会被处理。默认不额外过滤。
    // 例如按前缀过滤: [](const QFileInfo& fi){ return fi.fileName().startsWith("onnx", Qt::CaseInsensitive); }
    std::function<bool(const QFileInfo&)> filter = nullptr;

    // true = 移动（剪切），false = 复制
    bool moveInsteadOfCopy = false;

    // 解压完成后是否删除原始压缩包
    bool removeArchiveAfterExtract = true;

    // tar 等待超时时间（毫秒）
    int timeoutMs = 60000;

    int stallTimeoutMs = 30000;

    // 结构化解压进度回调（current/total/currentFile）。为 nullptr 时不回报。
    ExtractProgressCb onProgress = nullptr;
};

class ExtractTool {
public:
    static bool extractAndDeploy(const QString& archivePath, const ExtractOptions& options);

    static bool extractAll(const QString& archivePath,
                           const QString& destinationDir,
                           bool removeArchiveAfterExtract = true,
                           const ExtractProgressCb& onProgress = nullptr,
                           int stallTimeoutMs = 90000);
};
