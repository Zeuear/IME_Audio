#include <gtest/gtest.h>

#include "services/dictionary/LanguageScript.h"

using LanguageScript::Side;

TEST(LanguageScriptTest, FamilyNormalisesRegionScriptAndCantonese) {
    EXPECT_EQ(LanguageScript::family(QStringLiteral("zh-Hans")), QStringLiteral("zh"));
    EXPECT_EQ(LanguageScript::family(QStringLiteral("zh-Hant")), QStringLiteral("zh"));
    EXPECT_EQ(LanguageScript::family(QStringLiteral("yue")), QStringLiteral("zh"));
    EXPECT_EQ(LanguageScript::family(QStringLiteral("ES")), QStringLiteral("es"));
    EXPECT_EQ(LanguageScript::family(QStringLiteral("pt-BR")), QStringLiteral("pt"));
    EXPECT_EQ(LanguageScript::family(QString()), QString());
}

TEST(LanguageScriptTest, ContradictsOnlyWhenTheWritingSystemDoesNotBelong) {
    EXPECT_TRUE(LanguageScript::contradicts(QStringLiteral("密钥"), QStringLiteral("es")));
    EXPECT_FALSE(LanguageScript::contradicts(QStringLiteral("clave"), QStringLiteral("es")));
    EXPECT_TRUE(LanguageScript::contradicts(QStringLiteral("Привет"), QStringLiteral("zh")));
    EXPECT_TRUE(LanguageScript::contradicts(QStringLiteral("你好"), QStringLiteral("ru")));
    EXPECT_TRUE(LanguageScript::contradicts(QStringLiteral("한국어"), QStringLiteral("zh")));
    EXPECT_TRUE(LanguageScript::contradicts(QStringLiteral("日本語のテスト"), QStringLiteral("zh")));
    EXPECT_FALSE(LanguageScript::contradicts(QStringLiteral("日本語のテスト"), QStringLiteral("ja")));
    EXPECT_FALSE(LanguageScript::contradicts(QStringLiteral("한국어"), QStringLiteral("ko")));
}

TEST(LanguageScriptTest, LatinTermsAreAlwaysTolerated) {
    EXPECT_FALSE(LanguageScript::contradicts(QStringLiteral("API"), QStringLiteral("zh")));
    EXPECT_FALSE(LanguageScript::contradicts(QStringLiteral("Kuda"), QStringLiteral("ru")));
    EXPECT_FALSE(LanguageScript::contradicts(QStringLiteral("使用 API 密钥"), QStringLiteral("zh")));
}

TEST(LanguageScriptTest, MixedTextFollowsTheMajorityWithHanWeightedHigher) {
    EXPECT_TRUE(LanguageScript::contradicts(QStringLiteral("使用 API"), QStringLiteral("es")));
    EXPECT_FALSE(LanguageScript::contradicts(QStringLiteral("clave 密"), QStringLiteral("es")));
}

TEST(LanguageScriptTest, NoLettersOrUnknownLanguageNeverContradict) {
    EXPECT_FALSE(LanguageScript::contradicts(QStringLiteral("123 - 456"), QStringLiteral("es")));
    EXPECT_FALSE(LanguageScript::contradicts(QString(), QStringLiteral("zh")));
    EXPECT_FALSE(LanguageScript::contradicts(QStringLiteral("密钥"), QStringLiteral("xx")));
}

TEST(LanguageScriptTest, SideIsDecidedByScriptsOnlyOneLanguageUses) {
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("密钥"), QStringLiteral("zh"), QStringLiteral("es")), Side::A);
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("clave"), QStringLiteral("zh"), QStringLiteral("es")), Side::B);
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("감사"), QStringLiteral("zh"), QStringLiteral("ko")), Side::B);
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("ありがとう"), QStringLiteral("zh"), QStringLiteral("ja")), Side::B);
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("你好 hello"), QStringLiteral("zh"), QStringLiteral("en")), Side::A);
}

TEST(LanguageScriptTest, SharedScriptPairsStayUnknownWithoutAClearCue) {
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("hola"), QStringLiteral("es"), QStringLiteral("en")), Side::Unknown);
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("谢谢"), QStringLiteral("zh"), QStringLiteral("ja")), Side::Unknown);
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("привет"), QStringLiteral("ru"), QStringLiteral("uk")), Side::Unknown);
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("123"), QStringLiteral("zh"), QStringLiteral("es")), Side::Unknown);
    EXPECT_EQ(LanguageScript::sideOf(QString(), QStringLiteral("zh"), QStringLiteral("es")), Side::Unknown);
}

TEST(LanguageScriptTest, LatinCuesDecideOnlyWhenExclusiveToOneSide) {
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("¿qué?"), QStringLiteral("es"), QStringLiteral("en")), Side::A);
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("fenêtre"), QStringLiteral("fr"), QStringLiteral("en")), Side::A);
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("Straße"), QStringLiteral("en"), QStringLiteral("de")), Side::B);
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("window"), QStringLiteral("fr"), QStringLiteral("en")), Side::Unknown);
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("canción"), QStringLiteral("es"), QStringLiteral("pt")), Side::Unknown);
    EXPECT_EQ(LanguageScript::sideOf(QStringLiteral("coração"), QStringLiteral("es"), QStringLiteral("pt")), Side::B);
}
