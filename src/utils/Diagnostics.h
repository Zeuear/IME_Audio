#pragma once

#include <QString>

// 运行环境诊断：把"用户机器上到底是什么状态"落进日志文件。
// 用户反馈"加载失败"时，日志里必须能直接回答：跑的是哪个路径、哪个架构、
// 权限授了没有、模型目录里实际有哪些文件。
namespace Diagnostics {

// 接管 Qt 的 warning/critical 输出并写入日志文件；
// sherpa-onnx 的底层报错只走 stderr，Qt 的日志系统不会捕获。这个函数会把 stderr 重定向到日志文件。
void install();

// 启动时打印一次：版本、系统/架构、启动路径、隐私授权状态、音频设备。
void logEnvironment();

// 单个文件的存在性与大小。
void logFileState(const QString& label, const QString& path);

// 递归列出目录内容（含大小），最多 maxEntries 条。用于确认模型是否完整解压。
void logDirectoryListing(const QString& label, const QString& dir, int maxEntries = 40);

}
