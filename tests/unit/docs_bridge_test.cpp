#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include "services/docs/DocsBridge.h"

TEST(DocsBridgeTest, OnlyPinnedExtensionOriginAllowed)
{
    EXPECT_TRUE(DocsBridgeProtocol::isAllowedOrigin(DocsBridgeProtocol::kExtensionOrigin));
    EXPECT_TRUE(DocsBridgeProtocol::isAllowedOrigin(DocsBridgeProtocol::kExtensionOrigin + "/"));
    EXPECT_FALSE(DocsBridgeProtocol::isAllowedOrigin("chrome-extension://otherextensionid"));
    EXPECT_FALSE(DocsBridgeProtocol::isAllowedOrigin("https://docs.google.com"));
    EXPECT_FALSE(DocsBridgeProtocol::isAllowedOrigin(""));
}

TEST(DocsBridgeTest, TextMessageIsJsonWithTypeAndText)
{
    const QJsonObject o = QJsonDocument::fromJson(DocsBridgeProtocol::textMessage("你好\n\"x\"")).object();
    EXPECT_EQ(o.value("type").toString(), "text");
    EXPECT_EQ(o.value("text").toString(), "你好\n\"x\"");
}

// 配置里只存哈希：对得上原令牌，对不上别的值，也不能直接拿哈希当令牌用
TEST(DocsBridgeTest, TokenCheckedAgainstStoredHashOnly)
{
    const QByteArray stored = DocsBridgeProtocol::tokenHash("abc123");
    EXPECT_NE(stored, QByteArray("abc123"));
    EXPECT_TRUE(DocsBridgeProtocol::tokenMatches("abc123", stored));
    EXPECT_FALSE(DocsBridgeProtocol::tokenMatches("abc124", stored));
    EXPECT_FALSE(DocsBridgeProtocol::tokenMatches(QString::fromLatin1(stored), stored));
    EXPECT_FALSE(DocsBridgeProtocol::tokenMatches("", stored));
    EXPECT_FALSE(DocsBridgeProtocol::tokenMatches("abc123", {}));
}
