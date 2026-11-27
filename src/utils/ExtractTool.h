#pragma once
#include <functional>
#include <QApplication>
#include <QFileInfo>
#include "Logger.h"


struct ExtractProgress {
    int current = 0;
    int total = 1000;      // 进度分母，恒为 1000（条目总数要通读压缩包才知道，不值得）
    QString currentFile;
};

using ExtractProgressCb = std::function<void(const ExtractProgress&)>;

struct ExtractOptions
{
    QString destinationDir = QApplication::applicationDirPath();

    QStringList nameFilters = { "*.dll", "*.so*", "*.dylib" };

    // 返回 true 才会被处理。默认不额外过滤。
    std::function<bool(const QFileInfo&)> filter = nullptr;

    bool moveInsteadOfCopy = false;

    bool removeArchiveAfterExtract = true;

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
