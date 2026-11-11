#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include "services/DocsBridge.h"

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
