#pragma once
#include <QString>
#include <QStringList>

// @brief 系统防火墙里禁止本程序联网（出站拦截）的规则。
namespace Firewall {

enum class State {
    Missing,
    Blocked,
    StalePath,   // 规则指向别的路径（程序挪过位置）
};

inline QString ruleName() { return QStringLiteral("ImeAudioBlock"); }

bool isSupported();
State state(const QString& exePath);

// program 的值自带引号：路径可能有空格
inline QStringList addArgs(const QString& exePath)
{
    return { "advfirewall", "firewall", "add", "rule", "name=" + ruleName(), "dir=out", "action=block",
             "program=\"" + exePath + "\"", "enable=yes", "profile=any" };
}

inline QStringList updateArgs(const QString& exePath)
{
    return { "advfirewall", "firewall", "set", "rule", "name=" + ruleName(), "new",
             "program=\"" + exePath + "\"", "enable=yes" };
}

inline QStringList removeArgs()
{
    return { "advfirewall", "firewall", "delete", "rule", "name=" + ruleName() };
}

inline bool outputMentionsProgram(const QString& netshOutput, const QString& exePath)
{
    auto norm = [](QString s) { return s.replace(QLatin1Char('/'), QLatin1Char('\\')).toLower(); };
    return !exePath.isEmpty() && norm(netshOutput).contains(norm(exePath));
}

}
