#pragma once

#include <QString>

// 运行环境诊断：把用户机器状态（路径、架构、权限、模型文件）落进日志，便于排查加载失败。
namespace Diagnostics {

// 接管 Qt 的 warning/critical 输出写入日志；sherpa-onnx 底层报错只走 stderr，故把 stderr 也重定向到日志文件。
void install();

void logEnvironment();

void logFileState(const QString& label, const QString& path);

void logDirectoryListing(const QString& label, const QString& dir, int maxEntries = 40);

}
