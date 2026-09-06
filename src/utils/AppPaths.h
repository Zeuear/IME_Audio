#pragma once

#include <QString>

// 应用的文件位置集中在这里解析，其余代码不再自行拼接 applicationDirPath()。
//
// 区分两类根目录：
//   dataDir()     —— 可写数据（日志、配置、下载的模型、术语库、提示词）
//   resourceDir() —— 随应用分发的只读资源（内置 VAD 模型）
//
// Windows/Linux 上两者都是可执行文件所在目录，行为与历史版本完全一致。
// macOS 上 .app bundle 内部不可写（写入还会破坏签名），因此可写数据必须落到
// ~/Library/Application Support，只读资源则在 Contents/Resources。
namespace AppPaths {

QString dataDir();
QString resourceDir();

QString logFile();
QString configFile();
QString termsFile();
QString promptsDir();

QString sherpaRoot();
// 目录不存在时会创建
QString sherpaModelsDir();
QString vadModelFile();

}
