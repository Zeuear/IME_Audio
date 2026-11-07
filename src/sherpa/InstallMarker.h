#pragma once
#include <QString>

// @brief 模型安装完整性：用"安装中"标记文件区分"装好了"和"下到一半"。
//        以前 isInstalled 只看模型目录在不在，多文件模型下到一半被杀，已下完的几个文件在目录里，
//        下次启动就被当成已安装。这里反过来：开始下载时写标记，全部下完（含解压）才删除。
//        用"安装中"而不是"已完成"标记，是为了让升级前就装好的旧模型（没有任何标记）仍然算已安装，不用重新下载。
namespace InstallMarker {

// 目录存在，且没有"安装中"标记
bool isInstalled(const QString& modelDir);
// 目录存在，且带"安装中"标记：上一次下载没有完成
bool isIncomplete(const QString& modelDir);

// 开始下载前调用：创建目录（含缺少的父目录）并写入标记；失败返回 false
bool markInstalling(const QString& modelDir);
// 全部下完后调用：只删标记，不动模型文件；标记不在也不算错
void markComplete(const QString& modelDir);

} // namespace InstallMarker
