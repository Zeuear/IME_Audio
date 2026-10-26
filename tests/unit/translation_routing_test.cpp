#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include "translation/LanguageRouting.h"
#include "translation/RemoteSettingsIndex.h"

#if !defined(IME_AUDIO_TEST_DATA_DIR)
#define IME_AUDIO_TEST_DATA_DIR "tests/data"
#endif

static QString testDataPath(const QString& fileName) {
    return QDir(QString::fromLocal8Bit(IME_AUDIO_TEST_DATA_DIR)).filePath(fileName);
}

TEST(TranslationRoutingTest, LanguageDisplayNameMapping) {
    EXPECT_EQ(languageCodeForDisplayName(QStringLiteral("英文")), QStringLiteral("en"));
    EXPECT_EQ(languageCodeForDisplayName(QStringLiteral("日文")), QStringLiteral("ja"));
    EXPECT_EQ(languageCodeForDisplayName(QStringLiteral("韩文")), QStringLiteral("ko"));
    EXPECT_EQ(languageCodeForDisplayName(QStringLiteral("繁体中文")), QStringLiteral("zh-Hant"));
    EXPECT_EQ(languageCodeForDisplayName(QStringLiteral("简体中文")), QStringLiteral("zh-Hans"));
    EXPECT_TRUE(languageCodeForDisplayName(QStringLiteral("不翻译")).isEmpty());
    EXPECT_TRUE(languageCodeForDisplayName(QStringLiteral("未知语言")).isEmpty());

    EXPECT_EQ(displayNameForLanguageCode(QStringLiteral("en")), QStringLiteral("英文"));
    EXPECT_EQ(displayNameForLanguageCode(QStringLiteral("ja")), QStringLiteral("日文"));
    EXPECT_EQ(displayNameForLanguageCode(QStringLiteral("ko")), QStringLiteral("韩文"));
    EXPECT_EQ(displayNameForLanguageCode(QStringLiteral("zh-Hant")), QStringLiteral("繁体中文"));
    EXPECT_EQ(displayNameForLanguageCode(QStringLiteral("zh-Hans")), QStringLiteral("简体中文"));
    EXPECT_TRUE(displayNameForLanguageCode(QStringLiteral("xx")).isEmpty());
}

TEST(TranslationRoutingTest, PlanRouteSameLanguage) {
    QSet<QString> known;
    const auto same = planRoute(QStringLiteral("zh-Hans"), QStringLiteral("zh-Hans"), known);
    EXPECT_EQ(same.kind, RouteKind::Same);
    EXPECT_TRUE(same.hops.isEmpty());
}

TEST(TranslationRoutingTest, PlanRouteDirectEnglishPairs) {
    QSet<QString> known{ QStringLiteral("zh-Hans>en"), QStringLiteral("en>zh-Hans") };

    const auto zhToEn = planRoute(QStringLiteral("zh-Hans"), QStringLiteral("en"), known);
    EXPECT_EQ(zhToEn.kind, RouteKind::Direct);
    ASSERT_EQ(zhToEn.hops.size(), 1);
    EXPECT_EQ(zhToEn.hops.first().from, QStringLiteral("zh-Hans"));
    EXPECT_EQ(zhToEn.hops.first().to, QStringLiteral("en"));

    const auto enToZh = planRoute(QStringLiteral("en"), QStringLiteral("zh-Hans"), known);
    EXPECT_EQ(enToZh.kind, RouteKind::Direct);
    ASSERT_EQ(enToZh.hops.size(), 1);
    EXPECT_EQ(enToZh.hops.first().from, QStringLiteral("en"));
    EXPECT_EQ(enToZh.hops.first().to, QStringLiteral("zh-Hans"));
}

TEST(TranslationRoutingTest, PlanRoutePivotViaEnglish) {
    QSet<QString> known{ QStringLiteral("zh-Hans>en"), QStringLiteral("en>ja") };

    const auto route = planRoute(QStringLiteral("zh-Hans"), QStringLiteral("ja"), known);
    EXPECT_EQ(route.kind, RouteKind::Pivot);
    ASSERT_EQ(route.hops.size(), 2);
    EXPECT_EQ(route.hops.at(0).from, QStringLiteral("zh-Hans"));
    EXPECT_EQ(route.hops.at(0).to, QStringLiteral("en"));
    EXPECT_EQ(route.hops.at(1).from, QStringLiteral("en"));
    EXPECT_EQ(route.hops.at(1).to, QStringLiteral("ja"));
}

TEST(TranslationRoutingTest, PlanRouteMissingPivotIsUnsupported) {
    QSet<QString> known{ QStringLiteral("zh-Hans>en") }; // 缺 en>ja

    const auto route = planRoute(QStringLiteral("zh-Hans"), QStringLiteral("ja"), known);
    EXPECT_EQ(route.kind, RouteKind::Unsupported);
    EXPECT_TRUE(route.hops.isEmpty());
}

TEST(TranslationRoutingTest, PlanRouteUnknownDirectionIsUnsupported) {
    QSet<QString> known{ QStringLiteral("zh-Hans>en") };

    const auto route = planRoute(QStringLiteral("zh-Hans"), QStringLiteral("ko"), known);
    EXPECT_EQ(route.kind, RouteKind::Unsupported);
    EXPECT_TRUE(route.hops.isEmpty());
}

TEST(TranslationRoutingTest, PlanRouteEmptyLanguageIsUnsupported) {
    QSet<QString> known{ QStringLiteral("zh-Hans>en") };

    EXPECT_EQ(planRoute(QString(), QStringLiteral("en"), known).kind, RouteKind::Unsupported);
    EXPECT_EQ(planRoute(QStringLiteral("zh-Hans"), QString(), known).kind, RouteKind::Unsupported);
}

TEST(TranslationRemoteSettingsIndexTest, PicksNewestVersionPerDirection) {
    QFile file(testDataPath(QStringLiteral("translation_remote_settings_sample.json")));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QByteArray json = file.readAll();

    const QMap<QString, DirectionManifest> manifests =
        parseRemoteSettingsIndex(json, QStringLiteral("https://example.com/base/"));

    ASSERT_TRUE(manifests.contains(QStringLiteral("zh-Hans>en")));
    const DirectionManifest& zhToEn = manifests.value(QStringLiteral("zh-Hans>en"));
    EXPECT_EQ(zhToEn.version, QStringLiteral("1.10"));
    // 1.9 的旧版文件不应与 1.10 混在一起；只保留 1.10 的 model + lex
    ASSERT_EQ(zhToEn.files.size(), 2);
    EXPECT_EQ(zhToEn.files.at(0).fileType, QStringLiteral("model"));
    EXPECT_EQ(zhToEn.files.at(0).sha256, QStringLiteral("bbbb2222"));
    EXPECT_EQ(zhToEn.files.at(1).fileType, QStringLiteral("lex"));
    EXPECT_EQ(zhToEn.files.at(1).sha256, QStringLiteral("cccc3333"));

    // zh-Hans>ja 只有 1.0，也应被解析
    ASSERT_TRUE(manifests.contains(QStringLiteral("zh-Hans>ja")));
    EXPECT_EQ(manifests.value(QStringLiteral("zh-Hans>ja")).version, QStringLiteral("1.0"));

    // 缺字段的记录应被跳过，不生成 incomplete 方向
    EXPECT_FALSE(manifests.contains(QStringLiteral("incomplete>")));
}

TEST(TranslationRemoteSettingsIndexTest, AttachmentUrlUsesBaseUrl) {
    QFile file(testDataPath(QStringLiteral("translation_remote_settings_sample.json")));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QByteArray json = file.readAll();

    const QMap<QString, DirectionManifest> manifests =
        parseRemoteSettingsIndex(json, QStringLiteral("https://example.com/base/"));

    const DirectionManifest& zhToEn = manifests.value(QStringLiteral("zh-Hans>en"));
    EXPECT_EQ(zhToEn.files.at(0).url,
              QStringLiteral("https://example.com/base/translations-models/zh-Hans-en/1.10/model.bin"));

    // 默认基址仍能拼接
    const QMap<QString, DirectionManifest> defaults =
        parseRemoteSettingsIndex(json);
    EXPECT_TRUE(defaults.value(QStringLiteral("zh-Hans>en")).files.at(0).url.startsWith(
        QStringLiteral("https://firefox-settings-attachments.cdn.mozilla.net/")));
}

// 真实索引里约三分之二的记录没有顶层 hash，只有 attachment.hash
TEST(TranslationRemoteSettingsIndexTest, HashOnlyInAttachmentIsAccepted) {
    const QByteArray json = R"([{
        "fromLang": "en", "toLang": "be", "version": "2.0", "fileType": "vocab",
        "name": "vocab.enbe.spm",
        "attachment": { "hash": "abc123", "size": 10, "location": "x/vocab.spm" }
    }])";

    const auto manifests = parseRemoteSettingsIndex(json);
    ASSERT_TRUE(manifests.contains(QStringLiteral("en>be")));
    ASSERT_EQ(manifests.value(QStringLiteral("en>be")).files.size(), 1);
    EXPECT_EQ(manifests.value(QStringLiteral("en>be")).files.at(0).sha256, QStringLiteral("abc123"));
}

TEST(TranslationRemoteSettingsIndexTest, InvalidJsonIsEmpty) {
    EXPECT_TRUE(parseRemoteSettingsIndex(QStringLiteral("not json").toUtf8()).isEmpty());
    EXPECT_TRUE(parseRemoteSettingsIndex(QStringLiteral("{}").toUtf8()).isEmpty());
}

TEST(TranslationRoutingTest, StoredTargetAndSourceResolution) {
    EXPECT_EQ(languageCodeForStored(QStringLiteral("英文")), QStringLiteral("en"));
    EXPECT_EQ(languageCodeForStored(QStringLiteral("简体中文")), QStringLiteral("zh-Hans"));
    EXPECT_EQ(languageCodeForStored(QStringLiteral("de")), QStringLiteral("de"));

    EXPECT_EQ(resolveSourceCode(QStringLiteral("ja"), QStringLiteral("zh")), QStringLiteral("ja"));
    EXPECT_EQ(resolveSourceCode(QStringLiteral("auto"), QStringLiteral("zh")), QStringLiteral("zh-Hans"));
    EXPECT_EQ(resolveSourceCode(QStringLiteral("auto"), QStringLiteral("yue")), QStringLiteral("zh-Hant"));
    EXPECT_EQ(resolveSourceCode(QStringLiteral("auto"), QStringLiteral("fr")), QStringLiteral("fr"));
    EXPECT_TRUE(resolveSourceCode(QStringLiteral("auto"), {}).isEmpty());
}
