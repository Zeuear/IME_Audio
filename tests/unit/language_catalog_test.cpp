#include <gtest/gtest.h>

#include <QFile>
#include <QSet>

#include "sherpa/LanguageCatalog.h"
#include "translation/TranslationModelStore.h"

// 目标语言列表按语言代码到目录里查名称，查不到会退回显示英文名；
// 清单里新增语言而目录没跟上时，这里会提前失败
TEST(LanguageCatalogTest, EveryManifestLanguageHasACatalogName) {
    QFile f(QStringLiteral(IME_AUDIO_RESOURCES_DIR "/translation_models.json"));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));

    QSet<QString> codes;
    for (const DirectionManifest& m : TranslationModelStore::parseBundledManifest(f.readAll())) {
        codes << m.from << m.to;
    }
    codes.remove(QStringLiteral("zh-Hans"));   // 中文两种写法有专门的显示名
    codes.remove(QStringLiteral("zh-Hant"));

    for (const QString& code : codes) {
        EXPECT_FALSE(LanguageCatalog::keyOfCode(code).isEmpty()) << code.toStdString();
    }
}

// 目录里的名称是 UTF-8，按 Latin-1 读会变成乱码（BokmÃ¥l）
TEST(LanguageCatalogTest, NonAsciiNameIsDecodedAsUtf8) {
    EXPECT_EQ(LanguageCatalog::keyOfCode(QStringLiteral("nb")), QString::fromUtf8("Norwegian Bokmål"));
}
