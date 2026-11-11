#include <gtest/gtest.h>
#include "utils/Firewall.h"

TEST(FirewallTest, AddArgsQuotesProgramPathWithSpaces)
{
    const QStringList a = Firewall::addArgs("C:\\Program Files\\ImeAudio\\ImeAudio.exe");
    EXPECT_TRUE(a.contains("name=ImeAudioBlock"));
    EXPECT_TRUE(a.contains("dir=out"));
    EXPECT_TRUE(a.contains("action=block"));
    // 路径带空格：值自带引号，否则 netsh 会把它拆成两个参数
    EXPECT_TRUE(a.contains("program=\"C:\\Program Files\\ImeAudio\\ImeAudio.exe\""));
}

TEST(FirewallTest, RemoveAndUpdateArgsTargetSameRuleName)
{
    EXPECT_TRUE(Firewall::removeArgs().contains("name=ImeAudioBlock"));
    EXPECT_TRUE(Firewall::removeArgs().contains("delete"));
    const QStringList u = Firewall::updateArgs("D:\\app\\ImeAudio.exe");
    EXPECT_TRUE(u.contains("set"));
    EXPECT_TRUE(u.contains("name=ImeAudioBlock"));
    EXPECT_TRUE(u.contains("program=\"D:\\app\\ImeAudio.exe\""));
}

TEST(FirewallTest, OutputMatchIgnoresCaseAndSeparators)
{
    const QString out = "Rule Name: ImeAudio\nProgram: d:\\APP\\imeaudio.exe\n";
    EXPECT_TRUE(Firewall::outputMentionsProgram(out, "D:/app/ImeAudio.exe"));
    EXPECT_FALSE(Firewall::outputMentionsProgram(out, "D:\\other\\ImeAudio.exe"));
    EXPECT_FALSE(Firewall::outputMentionsProgram(out, ""));
}
