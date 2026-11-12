#pragma once
#include <QString>

// @brief Google 文档扩展随程序分发
namespace DocsExtension {

QString installDir();
bool exportTo(const QString& dir, const QString& token, QString* error);
QString pairingFileName();
// 读 dir 里已导出的令牌，没有返回空串
QString readToken(const QString& dir);

}
