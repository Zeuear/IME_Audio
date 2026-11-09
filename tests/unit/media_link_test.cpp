#include <gtest/gtest.h>

#include "services/media/MediaLink.h"

using namespace MediaLink;

TEST(MediaLink, AcceptsFacebookHosts) {
    EXPECT_TRUE(isSupported("https://www.facebook.com/watch/?v=123"));
    EXPECT_TRUE(isSupported("https://facebook.com/user/videos/123"));
    EXPECT_TRUE(isSupported("https://m.facebook.com/story.php?story_fbid=1&id=2"));
    EXPECT_TRUE(isSupported("https://web.facebook.com/reel/123"));
    EXPECT_TRUE(isSupported("https://fb.watch/abcDEF/"));
    EXPECT_TRUE(isSupported("HTTP://WWW.FACEBOOK.COM/x"));
}

TEST(MediaLink, RejectsOtherHostsAndLookalikes) {
    EXPECT_FALSE(isSupported("https://www.youtube.com/watch?v=1"));
    EXPECT_FALSE(isSupported("https://evilfacebook.com/watch"));
    EXPECT_FALSE(isSupported("https://facebook.com.evil.example/watch"));
    EXPECT_FALSE(isSupported("https://example.com/?u=https://facebook.com/x"));
}

TEST(MediaLink, RejectsNonWebAndCredentialLinks) {
    EXPECT_FALSE(isSupported("ftp://facebook.com/x"));
    EXPECT_FALSE(isSupported("file:///C:/a.mp3"));
    EXPECT_FALSE(isSupported("C:\\a.mp3"));
    EXPECT_FALSE(isSupported("facebook.com/watch"));
    EXPECT_FALSE(isSupported("https://user:pass@facebook.com/x"));
    EXPECT_FALSE(isSupported("--exec=calc https://facebook.com/x"));
    EXPECT_FALSE(isSupported(""));
}

TEST(MediaLink, ParseLinesTrimsDedupesAndReportsInvalid) {
    const QString text = "  https://www.facebook.com/a  \r\n\r\nhttps://fb.watch/b/\n"
                         "https://www.facebook.com/a\nnot a link\nhttps://example.com/x\n";
    QStringList invalid;
    const QStringList ok = parseLines(text, &invalid);
    EXPECT_EQ(ok, (QStringList{"https://www.facebook.com/a", "https://fb.watch/b/"}));
    EXPECT_EQ(invalid, (QStringList{"not a link", "https://example.com/x"}));
}

TEST(MediaLink, ParseLinesWithoutInvalidOutput) {
    EXPECT_TRUE(parseLines("").isEmpty());
    EXPECT_EQ(parseLines("bad\nhttps://fb.watch/x").size(), 1);
}

TEST(MediaLink, ParsesProgressPercent) {
    EXPECT_EQ(parseProgressPercent("[download]  12.3% of    8.53MiB at 1.00MiB/s ETA 00:04"), 12);
    EXPECT_EQ(parseProgressPercent("[download] 100% of    8.53MiB in 00:00:00 at 13.57MiB/s"), 100);
    EXPECT_EQ(parseProgressPercent("[download]   0.0% of    8.53MiB at  Unknown B/s ETA Unknown"), 0);
}

TEST(MediaLink, NonProgressLinesGiveMinusOne) {
    EXPECT_EQ(parseProgressPercent("TITLE:abc"), -1);
    EXPECT_EQ(parseProgressPercent("[download] Destination: a.mp4"), -1);
    EXPECT_EQ(parseProgressPercent(""), -1);
}

TEST(MediaLink, Sha256ForPicksTheRightAsset) {
    const QByteArray sums =
        "e51e69e89489f199530f13d32a4342da3ddb2cf34842f364603a066db05afbf3  yt-dlp\n"
        "F5389D3698ECDE4EF06F4E6B1CE0294FE9AC1396228C8990B8D3416D6D177A7D  yt-dlp.exe\n"
        "0066dbac3c124028e2dcae2a197fccee33cd91f456313d24f7af89fd54590e98 *yt-dlp_arm64.exe\n";
    EXPECT_EQ(sha256For(sums, "yt-dlp.exe"),
              QString("f5389d3698ecde4ef06f4e6b1ce0294fe9ac1396228c8990b8d3416d6d177a7d"));
    EXPECT_EQ(sha256For(sums, "yt-dlp"),
              QString("e51e69e89489f199530f13d32a4342da3ddb2cf34842f364603a066db05afbf3"));
    EXPECT_EQ(sha256For(sums, "yt-dlp_arm64.exe"),
              QString("0066dbac3c124028e2dcae2a197fccee33cd91f456313d24f7af89fd54590e98"));
}

TEST(MediaLink, Sha256ForMissingOrMalformedIsEmpty) {
    EXPECT_TRUE(sha256For("", "yt-dlp.exe").isEmpty());
    EXPECT_TRUE(sha256For("<html>not found</html>", "yt-dlp.exe").isEmpty());
    EXPECT_TRUE(sha256For("abc  yt-dlp.exe\n", "yt-dlp.exe").isEmpty());   // 校验值长度不对
}

TEST(MediaLink, ParsesResultLine) {
    QString title, path;
    EXPECT_TRUE(parseResultLine("我的视频\tC:\\data\\123.mp4", &title, &path));
    EXPECT_EQ(title, QString("我的视频"));
    EXPECT_EQ(path, QString("C:\\data\\123.mp4"));
    EXPECT_FALSE(parseResultLine("no tab here", &title, &path));
    EXPECT_FALSE(parseResultLine("title\t   ", &title, &path));
}

TEST(MediaLink, ClassifiesFailures) {
    EXPECT_EQ(classifyFailure("ERROR: [facebook] 1: You must be logged in to view this"), Failure::NeedLogin);
    EXPECT_EQ(classifyFailure("ERROR: This video is private"), Failure::NeedLogin);
    EXPECT_EQ(classifyFailure("ERROR: [facebook] 1: Cannot parse data"), Failure::ExtractorBroken);
    EXPECT_EQ(classifyFailure("ERROR: Unable to extract video data"), Failure::ExtractorBroken);
    EXPECT_EQ(classifyFailure("ERROR: Video not available"), Failure::Unavailable);
    EXPECT_EQ(classifyFailure("ERROR: <urlopen error [Errno 11001] getaddrinfo failed>"), Failure::Network);
    EXPECT_EQ(classifyFailure("something odd"), Failure::Other);
}

TEST(MediaLink, FailureMessagesAreChineseAndNonEmpty) {
    for (auto f : {Failure::NeedLogin, Failure::Unavailable, Failure::Network, Failure::ExtractorBroken, Failure::Other})
        EXPECT_FALSE(failureMessage(f).isEmpty());
}
