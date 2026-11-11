#include "utils/Firewall.h"
#include "utils/Logger.h"
#include <QDir>
#include <QProcess>

bool Firewall::isSupported() { return true; }

Firewall::State Firewall::state(const QString& exePath)
{
    QProcess p;
    p.start("netsh", { "advfirewall", "firewall", "show", "rule", "name=" + ruleName(), "verbose" });
    if (!p.waitForFinished(8000)) {
        LOG_WARN("Firewall: netsh show rule timed out");
        p.kill();
        return State::Missing;
    }
    // 无规则时 netsh 返回非零；输出文字随系统语言变，不能依赖
    if (p.exitCode() != 0) return State::Missing;
    const QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
    return outputMentionsProgram(out, QDir::toNativeSeparators(exePath)) ? State::Blocked : State::StalePath;
}
