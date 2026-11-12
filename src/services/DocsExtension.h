#pragma once
#include <QString>

// @brief Google 文档扩展随程序分发
namespace DocsExtension {

QString installDir();
bool exportTo(const QString& dir, const QString& token, QString* error);

QString guideFileName();
QString pairingFileName();

}
