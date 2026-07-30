#pragma once
#include <functional>
#include <QApplication>
#include <QFileInfo>
#include "Logger.h"


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
    std::function<void(const QString&)> onProgressLine = [](const QString&msg) {
        LOG_DEBUG(msg);
    };
};

class ExtractTool {
public:
    static bool extractAndDeploy(const QString& archivePath, const ExtractOptions& options);

    static bool extractAll(const QString& archivePath,
                           const QString& destinationDir,
                           bool removeArchiveAfterExtract = true,
                           const std::function<void(const QString&)>& onProgressLine = nullptr,
                           int stallTimeoutMs = 90000);
};


