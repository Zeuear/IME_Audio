#pragma once
// 预编译头：只放几乎每个 cpp 都会用到、且基本不会变的 Qt 公共头，省掉重复解析。
// 不要放项目自己的头，也不要放 sherpa 的头——它们一变就会让全部源文件重编。
// 注意：有了它，某个 cpp 漏写 #include 也可能照样编过；以不带预编译头的构建为准（见 IMEAUDIO_USE_PCH）。
#ifdef __cplusplus
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVector>
#include <QDateTime>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#endif
