#pragma once
#include <functional>
#include <QApplication>
#include <QFileInfo>
#include "Logger.h"


struct ExtractProgress {
    int current = 0;       // 进度分子：压缩包已读字节折算的千分比（0..1000）
    int total = 1000;      // 进度分母，恒为 1000（条目总数要通读压缩包才知道，不值得）
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

    // 结构化解压进度回调（current/total/currentFile）。为 nullptr 时不回报。
    ExtractProgressCb onProgress = nullptr;
};

// @brief 基于 libarchive 的进程内解压（tar.bz2 / tar.gz / zip），不依赖系统 tar。
//        失败原因通过 error 带回，供界面直接展示。
class ExtractTool {
public:
    static bool extractAndDeploy(const QString& archivePath, const ExtractOptions& options,
                                 QString* error = nullptr);

    // 目录里（忽略以 "." 开头的标记文件）只有一个子目录时，把它的内容上移一层并删掉该目录。
    // wrapperName 非空时，只有子目录恰好叫这个名字才处理（用于修复已经装成 X/X 的旧安装）。
    static bool flattenWrapper(const QString& dir, const QString& wrapperName = QString());

    static bool extractAll(const QString& archivePath,
                           const QString& destinationDir,
                           bool removeArchiveAfterExtract = true,
                           const ExtractProgressCb& onProgress = nullptr,
                           QString* error = nullptr);
};
