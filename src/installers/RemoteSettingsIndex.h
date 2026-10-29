#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <cstdint>

// @brief 把 Mozilla Remote Settings 的模型索引记录解析成"方向 -> 可用文件清单"。
struct ModelFile {
    QString fileType; // model / lex / vocab / srcvocab / trgvocab
    QString name;
    QString sha256;
    qint64 size = 0;
    QString url;
};

struct DirectionManifest {
    QString from;
    QString to;
    QString version; 
    QList<ModelFile> files;
};

// 解析 Remote Settings 记录数组；同方向只保留 version 最大的一组，版本比较语义化
//（1.10 > 1.9）。缺字段的记录跳过，不崩溃。
QMap<QString, DirectionManifest> parseRemoteSettingsIndex(const QByteArray& json,
                                                          const QString& attachmentBaseUrl =
                                                              QStringLiteral(
                                                                  "https://firefox-settings-attachments.cdn.mozilla.net/"));
