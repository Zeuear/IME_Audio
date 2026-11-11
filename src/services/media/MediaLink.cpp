#include "MediaLink.h"
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace MediaLink {

bool isSupported(const QString& url)
{
    const QUrl u(url, QUrl::StrictMode);
    if (!u.isValid() || u.isRelative()) return false;
    const QString scheme = u.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) return false;
    // 带账号密码的不是正常贴文链接，不能把凭据交给外部程序
    if (!u.userInfo().isEmpty()) return false;

    const QString host = u.host().toLower();
    const auto isDomainOrSubdomain = [&host](const QLatin1String& domain) {
        return host == domain || host.endsWith(QLatin1Char('.') + QString(domain));
    };
    return host == QLatin1String("fb.watch")
        || isDomainOrSubdomain(QLatin1String("facebook.com"))
        || host == QLatin1String("youtu.be")
        || isDomainOrSubdomain(QLatin1String("youtube.com"))
        || isDomainOrSubdomain(QLatin1String("youtube-nocookie.com"));
}

QStringList parseLines(const QString& text, QStringList* invalid)
{
    QStringList ok;
    QSet<QString> seen;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || seen.contains(line)) continue;
        seen.insert(line);
        if (isSupported(line)) ok << line;
        else if (invalid) *invalid << line;
    }
    return ok;
}

int parseProgressPercent(const QString& line)
{
    static const QRegularExpression re(QStringLiteral("^\\[download\\]\\s+(\\d+(?:\\.\\d+)?)%"));
    const auto m = re.match(line.trimmed());
    if (!m.hasMatch()) return -1;
    return qBound(0, static_cast<int>(m.captured(1).toDouble()), 100);
}

QString sha256For(const QByteArray& sums, const QString& assetName)
{
    const QStringList lines = QString::fromUtf8(sums).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        // 形如 "<64位十六进制>  文件名"，二进制模式时文件名前是 '*'
        const int sep = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
        if (sep != 64) continue;
        QString name = line.mid(sep).trimmed();
        if (name.startsWith(QLatin1Char('*'))) name.remove(0, 1);
        if (name == assetName) return line.left(64).toLower();
    }
    return {};
}

bool parseResultLine(const QString& line, QString* title, QString* path)
{
    const int tab = line.indexOf(QLatin1Char('\t'));
    if (tab < 0) return false;
    const QString p = line.mid(tab + 1).trimmed();
    if (p.isEmpty()) return false;
    if (title) *title = line.left(tab).trimmed();
    if (path) *path = p;
    return true;
}

Failure classifyFailure(const QString& stderrText)
{
    const QString s = stderrText.toLower();
    auto has = [&](const char* needle) { return s.contains(QLatin1String(needle)); };

    if (has("login") || has("log in") || has("sign in") || has("private") || has("cookies") || has("registered users")
        || has("you must be logged") || has("this video is only available for"))
        return Failure::NeedLogin;
    if (has("unable to extract") || has("cannot parse data") || has("unsupported url") || has("please report this issue")
        || (has("unable to download webpage") && has("http error 4")))
        return Failure::ExtractorBroken;
    if (has("not available") || has("been removed") || has("deleted") || has("no video") || has("does not exist"))
        return Failure::Unavailable;
    if (has("timed out") || has("timeout") || has("getaddrinfo") || has("connection") || has("network is unreachable")
        || has("temporary failure") || has("ssl"))
        return Failure::Network;
    return Failure::Other;
}

QString failureMessage(Failure failure)
{
    switch (failure) {
    case Failure::NeedLogin:
        return QStringLiteral("该链接需要登录才能访问（私密或好友可见），目前只支持公开链接");
    case Failure::Unavailable:
        return QStringLiteral("内容不可用，可能已被删除或设为不公开");
    case Failure::Network:
        return QStringLiteral("网络连接失败，请检查网络后重试");
    case Failure::ExtractorBroken:
        // yt-dlp 对"视频不存在 / 非公开"和"站点改版"报的是同一种错，这里不能只怪其中一种
        return QStringLiteral("无法解析该链接：内容可能已删除或不是公开的，也可能是平台页面有变化（已检查下载组件更新）");
    case Failure::Other:
        break;
    }
    return QStringLiteral("下载失败，详细信息见日志");
}

}
