#pragma once
#include <QString>

// @brief 用"安装中"标记区分已装好与下到一半；旧模型无标记仍算已安装，不必重下。
namespace InstallMarker {

bool isInstalled(const QString& modelDir);
// 目录存在，且带"安装中"标记：上一次下载没有完成
bool isIncomplete(const QString& modelDir);

// 开始下载前调用：创建目录（含缺少的父目录）并写入标记；失败返回 false
bool markInstalling(const QString& modelDir);
// 全部下完后调用：只删标记，不动模型文件；标记不在也不算错
void markComplete(const QString& modelDir);

} // namespace InstallMarker
