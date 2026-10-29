#pragma once

#include "RemoteSettingsIndex.h"

#include <QMap>
#include <QString>

// @brief 翻译模型在磁盘上的布局、完整性校验和 Bergamot 配置生成；纯文件逻辑，不联网。
namespace TranslationModelStore {

// 随程序发布的清单（resources/translation_models.json）
QMap<QString, DirectionManifest> parseBundledManifest(const QByteArray& json);

QString directionDir(const QString& modelsRoot, const QString& from, const QString& to);
// .part 是下载中的临时名，校验通过后才改成正式名，避免半截文件被当成已安装
QString partPath(const QString& finalPath);

// 所有文件都在且大小与清单一致才算已安装
bool isInstalled(const QString& modelsRoot, const DirectionManifest& manifest);
qint64 totalBytes(const DirectionManifest& manifest);

bool verifySha256(const QString& path, const QString& expectedHex);

// 校验每个 .part 并改名；任何一个失败都清理该方向的 .part，返回 false 并写入原因
bool finalizeDownload(const QString& modelsRoot, const DirectionManifest& manifest, QString* error);

// 生成 Bergamot 的 YAML 配置，参数与 Firefox 一致（贪心、int8）；路径用正斜杠绝对路径
QString generateBergamotYaml(const QString& modelsRoot, const DirectionManifest& manifest);

}
