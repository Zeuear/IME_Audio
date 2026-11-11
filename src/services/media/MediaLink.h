#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>

// @brief
// 链接转录的纯逻辑：链接白名单、yt-dlp 输出解析、失败归类。不碰网络和进程，便于单元测试。
namespace MediaLink {

// 只接受已知平台的公开链接，避免把任意 URL 交给 yt-dlp。
bool isSupported(const QString& url);

QStringList parseLines(const QString& text, QStringList* invalid = nullptr);

// 不是进度行返回 -1
int parseProgressPercent(const QString& line);

// 找不到返回空串
QString sha256For(const QByteArray& sums, const QString& assetName);

// 一行格式为 "标题\t路径"
bool parseResultLine(const QString& line, QString* title, QString* path);

enum class Failure {
    NeedLogin,
    Unavailable,
    Network,
    ExtractorBroken,   // yt-dlp 解析失败，值得先更新它再重试
    Other,
};
Failure classifyFailure(const QString& stderrText);

QString failureMessage(Failure failure);

}
