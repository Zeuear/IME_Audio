#include <gtest/gtest.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>

#include "translation/TranslationModelStore.h"

using namespace TranslationModelStore;

namespace {

QByteArray sha256Hex(const QByteArray& data) {
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

void writeFile(const QString& path, const QByteArray& data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(data);
}

// 两个文件的假方向：en>de，model 和 vocab
DirectionManifest makeManifest(const QByteArray& model, const QByteArray& vocab) {
    DirectionManifest m;
    m.from = QStringLiteral("en");
    m.to = QStringLiteral("de");
    m.version = QStringLiteral("1.0");
    m.files.append({ QStringLiteral("model"), QStringLiteral("model.ende.bin"),
                     QString::fromLatin1(sha256Hex(model)), model.size(), {} });
    m.files.append({ QStringLiteral("vocab"), QStringLiteral("vocab.ende.spm"),
                     QString::fromLatin1(sha256Hex(vocab)), vocab.size(), {} });
    return m;
}

} // namespace

TEST(TranslationModelStoreTest, ParseBundledManifestJoinsBaseUrl) {
    const QByteArray json = R"({
        "baseUrl": "https://cdn.example/",
        "directions": {
            "en>de": { "version": "1.0", "files": [
                { "type": "model", "name": "model.ende.bin", "sha256": "ab", "size": 5, "location": "x/model.bin" }
            ]},
            "bad-key": { "version": "1.0", "files": [] }
        }
    })";

    const auto manifests = parseBundledManifest(json);
    ASSERT_EQ(manifests.size(), 1);
    const DirectionManifest& m = manifests.value(QStringLiteral("en>de"));
    EXPECT_EQ(m.from, QStringLiteral("en"));
    EXPECT_EQ(m.to, QStringLiteral("de"));
    ASSERT_EQ(m.files.size(), 1);
    EXPECT_EQ(m.files.at(0).url, QStringLiteral("https://cdn.example/x/model.bin"));
    EXPECT_EQ(m.files.at(0).size, 5);
}

// 随程序发布的清单：每个方向都得有能跑起来的完整文件集，否则下载完也用不了
TEST(TranslationModelStoreTest, BundledManifestIsComplete) {
    QFile f(QStringLiteral(IME_AUDIO_RESOURCES_DIR "/translation_models.json"));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));

    const auto manifests = parseBundledManifest(f.readAll());
    EXPECT_GE(manifests.size(), 100);

    const QRegularExpression hex(QStringLiteral("^[0-9a-f]{64}$"));
    for (const DirectionManifest& m : manifests) {
        QStringList types;
        for (const ModelFile& file : m.files) {
            types << file.fileType;
            EXPECT_TRUE(hex.match(file.sha256).hasMatch()) << m.from.toStdString() << ">" << m.to.toStdString();
            EXPECT_GT(file.size, 0);
            EXPECT_TRUE(file.url.startsWith(QStringLiteral("https://")));
        }
        EXPECT_TRUE(types.contains(QStringLiteral("model"))) << m.from.toStdString() << ">" << m.to.toStdString();
        EXPECT_TRUE(types.contains(QStringLiteral("lex"))) << m.from.toStdString() << ">" << m.to.toStdString();
        const bool shared = types.contains(QStringLiteral("vocab"));
        const bool split = types.contains(QStringLiteral("srcvocab")) && types.contains(QStringLiteral("trgvocab"));
        EXPECT_TRUE(shared || split) << m.from.toStdString() << ">" << m.to.toStdString();
    }

    // 中英互译都在
    EXPECT_TRUE(manifests.contains(QStringLiteral("zh-Hans>en")));
    EXPECT_TRUE(manifests.contains(QStringLiteral("en>zh-Hans")));
}

TEST(TranslationModelStoreTest, InstalledNeedsEveryFileWithExpectedSize) {
    QTemporaryDir root;
    const DirectionManifest m = makeManifest("MODEL", "VOCAB");
    const QString dir = directionDir(root.path(), m.from, m.to);

    EXPECT_FALSE(isInstalled(root.path(), m));

    writeFile(dir + "/model.ende.bin", "MODEL");
    EXPECT_FALSE(isInstalled(root.path(), m));   // 缺一个文件

    writeFile(dir + "/vocab.ende.spm", "VOC");
    EXPECT_FALSE(isInstalled(root.path(), m));   // 大小不对

    writeFile(dir + "/vocab.ende.spm", "VOCAB");
    EXPECT_TRUE(isInstalled(root.path(), m));
}

TEST(TranslationModelStoreTest, VerifySha256) {
    QTemporaryDir root;
    const QString path = root.filePath("a.bin");
    writeFile(path, "hello");

    EXPECT_TRUE(verifySha256(path, QString::fromLatin1(sha256Hex("hello"))));
    EXPECT_TRUE(verifySha256(path, QString::fromLatin1(sha256Hex("hello")).toUpper()));
    EXPECT_FALSE(verifySha256(path, QString::fromLatin1(sha256Hex("hellO"))));
    EXPECT_FALSE(verifySha256(root.filePath("missing.bin"), QString::fromLatin1(sha256Hex("hello"))));
}

TEST(TranslationModelStoreTest, FinalizeRenamesVerifiedPartFiles) {
    QTemporaryDir root;
    const DirectionManifest m = makeManifest("MODEL", "VOCAB");
    const QString dir = directionDir(root.path(), m.from, m.to);
    writeFile(partPath(dir + "/model.ende.bin"), "MODEL");
    writeFile(partPath(dir + "/vocab.ende.spm"), "VOCAB");

    QString error;
    EXPECT_TRUE(finalizeDownload(root.path(), m, &error)) << error.toStdString();
    EXPECT_TRUE(isInstalled(root.path(), m));
    EXPECT_FALSE(QFile::exists(partPath(dir + "/model.ende.bin")));
}

TEST(TranslationModelStoreTest, FinalizeRejectsTamperedFileAndCleansUp) {
    QTemporaryDir root;
    const DirectionManifest m = makeManifest("MODEL", "VOCAB");
    const QString dir = directionDir(root.path(), m.from, m.to);
    writeFile(partPath(dir + "/model.ende.bin"), "MODEL");
    writeFile(partPath(dir + "/vocab.ende.spm"), "VOCAX");   // 内容被改

    QString error;
    EXPECT_FALSE(finalizeDownload(root.path(), m, &error));
    EXPECT_TRUE(error.contains(QStringLiteral("校验失败")));
    // 半截文件不能留下，也不能被当成已安装
    EXPECT_FALSE(isInstalled(root.path(), m));
    EXPECT_FALSE(QFile::exists(partPath(dir + "/model.ende.bin")));
    EXPECT_FALSE(QFile::exists(partPath(dir + "/vocab.ende.spm")));
}

TEST(TranslationModelStoreTest, YamlUsesSharedVocabForEuropeanTarget) {
    QTemporaryDir root;
    const DirectionManifest m = makeManifest("M", "V");
    const QString yaml = generateBergamotYaml(root.path(), m);
    const QString dir = QDir::fromNativeSeparators(directionDir(root.path(), "en", "de"));

    EXPECT_TRUE(yaml.contains(QStringLiteral("models: [%1/model.ende.bin]").arg(dir)));
    EXPECT_TRUE(yaml.contains(QStringLiteral("vocabs: [%1/vocab.ende.spm, %1/vocab.ende.spm]").arg(dir)));
    EXPECT_TRUE(yaml.contains(QStringLiteral("gemm-precision: int8shiftAlphaAll")));
    EXPECT_TRUE(yaml.contains(QStringLiteral("ssplit-mode: paragraph")));
}

TEST(TranslationModelStoreTest, YamlSplitsSourceAndTargetVocabForCjkTarget) {
    QTemporaryDir root;
    DirectionManifest m;
    m.from = QStringLiteral("en");
    m.to = QStringLiteral("zh-Hans");
    m.files = {
        { QStringLiteral("model"), QStringLiteral("model.enzh.bin"), {}, 1, {} },
        { QStringLiteral("lex"), QStringLiteral("lex.enzh.bin"), {}, 1, {} },
        { QStringLiteral("srcvocab"), QStringLiteral("srcvocab.enzh.spm"), {}, 1, {} },
        { QStringLiteral("trgvocab"), QStringLiteral("trgvocab.enzh.spm"), {}, 1, {} },
    };

    const QString yaml = generateBergamotYaml(root.path(), m);
    const QString dir = QDir::fromNativeSeparators(directionDir(root.path(), "en", "zh-Hans"));

    EXPECT_TRUE(yaml.contains(QStringLiteral("vocabs: [%1/srcvocab.enzh.spm, %1/trgvocab.enzh.spm]").arg(dir)));
    EXPECT_TRUE(yaml.contains(QStringLiteral("shortlist: [%1/lex.enzh.bin, false]").arg(dir)));
}
