#pragma once
#include <QString>

// @brief Google 文档扩展随程序分发：文件嵌在资源里，需要时导出成 Chrome 能"加载已解压的扩展程序"的文件夹。
// Chrome 不接受商店以外的 crx，所以只能给用户一个文件夹和一份安装说明。
namespace DocsExtension {

// 导出位置固定，Chrome 每次启动都从这里加载，重复导出就是原地更新
QString installDir();

// 把资源里的扩展文件写到 dir（覆盖旧文件）。失败返回 false，error 为中文原因
bool exportTo(const QString& dir, QString* error);

// 安装说明在导出目录里的文件名
QString guideFileName();

} // namespace DocsExtension
