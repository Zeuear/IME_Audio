#pragma once

#include <QString>

// macOS 上 .app bundle 内部不可写（写入还会破坏签名），可写数据必须落到
// Application Support，只读资源在 Contents/Resources；Windows/Linux 上两者
// 都是可执行文件所在目录。新增文件时务必按可写/只读归类，放错在 macOS 上
// 表现为运行时找不到或写不进，编译与 CI 都发现不了。
namespace AppPaths {

// 可写：日志、配置、下载的模型、术语库、提示词
QString dataDir();
// 只读、随应用分发：内置 VAD 模型
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
