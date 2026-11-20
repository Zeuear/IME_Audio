#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QFileInfo>
#include <archive.h>
#include <archive_entry.h>
#include <QTemporaryDir>
#include <QString>

#include "utils/ExtractTool.h"

// Helper: create N files in dir, then tar them into archivePath.
// Returns the number of regular files placed inside the archive.
static int makeTarArchive(const QString& dir, const QString& archivePath, int n)
{
    QDir d(dir);
    for (int i = 0; i < n; ++i) {
        QFile f(d.filePath(QString("file_%1.txt").arg(i)));
        EXPECT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(QByteArray("hello ").append(QByteArray::number(i)));
        f.close();
    }
    QProcess tar;
    tar.setProgram("tar");
    tar.setArguments({ "cf", archivePath, "-C", dir, "." });
    tar.start();
    EXPECT_TRUE(tar.waitForFinished(30000));
    EXPECT_EQ(tar.exitCode(), 0);
    return n;
}

class ExtractProgressTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!qApp) {
            static int argc = 1;
            static char* argv[] = { const_cast<char*>("tests"), nullptr };
            m_app = new QCoreApplication(argc, argv);
        }
    }
    QCoreApplication* m_app = nullptr;
};

// 用 libarchive 自己写档，保证测试不依赖系统 tar / bzip2（这正是要回归的那条路径）
enum class Pack { TarBz2, TarGz, Zip };

static bool writeArchive(const QString& path, Pack pack, const QList<QPair<QString, QByteArray>>& files)
{
    archive* a = archive_write_new();
    switch (pack) {
    case Pack::TarBz2: archive_write_set_format_pax_restricted(a); archive_write_add_filter_bzip2(a); break;
    case Pack::TarGz:  archive_write_set_format_pax_restricted(a); archive_write_add_filter_gzip(a);  break;
    case Pack::Zip:    archive_write_set_format_zip(a); break;
    }
    if (archive_write_open_filename(a, QFile::encodeName(path).constData()) != ARCHIVE_OK) {
        archive_write_free(a);
        return false;
    }
    for (const auto& f : files) {
        archive_entry* e = archive_entry_new();
        archive_entry_set_pathname(e, f.first.toUtf8().constData());
        archive_entry_set_size(e, f.second.size());
        archive_entry_set_filetype(e, AE_IFREG);
        archive_entry_set_perm(e, 0644);
        archive_write_header(a, e);
        archive_write_data(a, f.second.constData(), size_t(f.second.size()));
        archive_entry_free(e);
    }
    archive_write_free(a);
    return true;
}

static QByteArray readAllBytes(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

TEST_F(ExtractProgressTest, ExtractsEveryPackFormatWithoutExternalTools) {
    const QList<QPair<Pack, QString>> packs = {
        { Pack::TarBz2, "a.tar.bz2" }, { Pack::TarGz, "a.tar.gz" }, { Pack::Zip, "a.zip" } };
    for (const auto& [pack, name] : packs) {
        QTemporaryDir tmp;
        ASSERT_TRUE(tmp.isValid());
        const QString archive = tmp.filePath(name);
        const QByteArray payload(300 * 1024, 'x');
        ASSERT_TRUE(writeArchive(archive, pack, { { "model/tokens.txt", "abc" }, { "model/big.bin", payload } })) << name.toStdString();

        const QString dest = tmp.filePath("out");
        QString error;
        ASSERT_TRUE(ExtractTool::extractAll(archive, dest, true, nullptr, &error)) << name.toStdString() << ": " << error.toStdString();
        // 单层包裹目录会被剥掉
        EXPECT_EQ(readAllBytes(dest + "/tokens.txt"), QByteArray("abc")) << name.toStdString();
        EXPECT_EQ(readAllBytes(dest + "/big.bin"), payload) << name.toStdString();
        EXPECT_FALSE(QFileInfo::exists(archive)) << "archive should be removed after success";
    }
}

TEST_F(ExtractProgressTest, ProgressIsMonotonicPermilleEndingAtThousand) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString archive = tmp.filePath("p.tar.bz2");
    // 随机数据压不动，保证压缩包足够大、数据块回调多次
    QByteArray noisy(2 * 1024 * 1024, 0);
    for (int i = 0; i < noisy.size(); ++i) noisy[i] = char((i * 2654435761u) >> 13);
    ASSERT_TRUE(writeArchive(archive, Pack::TarBz2, { { "d/a.bin", noisy } }));

    int last = -1, calls = 0, total = 0;
    ASSERT_TRUE(ExtractTool::extractAll(archive, tmp.filePath("out"), false,
        [&](const ExtractProgress& p) { EXPECT_GE(p.current, last); last = p.current; total = p.total; ++calls; }));
    EXPECT_GE(calls, 2);
    EXPECT_EQ(total, 1000);
    EXPECT_EQ(last, 1000);
}

TEST_F(ExtractProgressTest, RejectsPathTraversalEntry) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString archive = tmp.filePath("evil.tar.gz");
    ASSERT_TRUE(writeArchive(archive, Pack::TarGz, { { "../evil.txt", "pwned" } }));

    QString error;
    EXPECT_FALSE(ExtractTool::extractAll(archive, tmp.filePath("out"), true, nullptr, &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(QFileInfo::exists(tmp.filePath("evil.txt")));
    EXPECT_TRUE(QFileInfo::exists(archive)) << "failed extraction must keep the archive";
}

TEST_F(ExtractProgressTest, CorruptArchiveReportsReadableError) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString archive = tmp.filePath("bad.tar.bz2");
    QFile f(archive);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(QByteArray(4096, 'z'));
    f.close();

    QString error;
    EXPECT_FALSE(ExtractTool::extractAll(archive, tmp.filePath("out"), true, nullptr, &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(QFileInfo::exists(tmp.filePath("out"))) << "half-made destination should be cleaned";
}

TEST_F(ExtractProgressTest, DeployFiltersByNameAndPrefix) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString archive = tmp.filePath("rt.tar.bz2");
    ASSERT_TRUE(writeArchive(archive, Pack::TarBz2, {
        { "lib/onnxruntime_providers_cuda.dll", "cuda" }, { "lib/other.dll", "no" }, { "lib/readme.txt", "no" } }));

    ExtractOptions opts;
    opts.destinationDir = tmp.filePath("app");
    opts.filter = [](const QFileInfo& fi) { return fi.fileName().startsWith("onnxruntime_providers_"); };
    QString error;
    ASSERT_TRUE(ExtractTool::extractAndDeploy(archive, opts, &error)) << error.toStdString();
    EXPECT_TRUE(QFileInfo::exists(opts.destinationDir + "/onnxruntime_providers_cuda.dll"));
    EXPECT_FALSE(QFileInfo::exists(opts.destinationDir + "/other.dll"));
    EXPECT_FALSE(QFileInfo::exists(opts.destinationDir + "/readme.txt"));
}

// 模型安装的真实形态：目标目录 models/X 事先建好，压缩包顶层目录也叫 X，里面还有子目录。
// 必须摊平成 models/X/<文件>，否则 sherpa 在 models/X 下找不到模型文件，识别器悄悄失效、转录恒为空。
TEST_F(ExtractProgressTest, PrecreatedTargetWithSameNamedWrapperIsFlattened) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString archive = tmp.filePath("m.tar.bz2");
    const QByteArray payload(2 * 1024 * 1024, 'q');
    ASSERT_TRUE(writeArchive(archive, Pack::TarBz2, {
        { "model-x/conv_frontend.onnx", payload }, { "model-x/decoder.int8.onnx", payload },
        { "model-x/tokenizer/vocab.json", "{}" }, { "model-x/test_wavs/a.wav", "w" }, { "model-x/README.md", "r" } }));

    const QString dest = tmp.filePath("models/model-x");
    ASSERT_TRUE(QDir().mkpath(dest));
    QString error;
    ASSERT_TRUE(ExtractTool::extractAll(archive, dest, true, nullptr, &error)) << error.toStdString();
    EXPECT_TRUE(QFileInfo::exists(dest + "/conv_frontend.onnx"));
    EXPECT_TRUE(QFileInfo::exists(dest + "/tokenizer/vocab.json"));
    EXPECT_FALSE(QFileInfo::exists(dest + "/model-x")) << "wrapper directory must not remain nested";
}

// 回归：安装流程会先在目标目录建 .installing 标记，解压时它和包裹目录同时存在。
// 标记不能算作"目录里还有别的东西"，否则不摊平，模型留在 X/X/ 下，转录恒为空。
TEST_F(ExtractProgressTest, InstallMarkerDoesNotPreventFlatteningWrapper) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString archive = tmp.filePath("m.tar.bz2");
    ASSERT_TRUE(writeArchive(archive, Pack::TarBz2, { { "model-x/conv_frontend.onnx", "c" }, { "model-x/tokenizer/vocab.json", "{}" } }));

    const QString dest = tmp.filePath("models/model-x");
    ASSERT_TRUE(QDir().mkpath(dest));
    { QFile marker(dest + "/.installing"); ASSERT_TRUE(marker.open(QIODevice::WriteOnly)); }
    QString error;
    ASSERT_TRUE(ExtractTool::extractAll(archive, dest, true, nullptr, &error)) << error.toStdString();
    EXPECT_TRUE(QFileInfo::exists(dest + "/conv_frontend.onnx"));
    EXPECT_TRUE(QFileInfo::exists(dest + "/tokenizer/vocab.json"));
    EXPECT_FALSE(QFileInfo::exists(dest + "/model-x"));
    EXPECT_TRUE(QFileInfo::exists(dest + "/.installing")) << "marker belongs to the installer, must stay until markComplete";
}

TEST_F(ExtractProgressTest, FlattenRepairsAlreadyNestedInstallButOnlyForMatchingName) {
    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    const QString dir = tmp.filePath("model-x");
    ASSERT_TRUE(QDir().mkpath(dir + "/model-x/tokenizer"));
    { QFile f(dir + "/model-x/conv_frontend.onnx"); ASSERT_TRUE(f.open(QIODevice::WriteOnly)); f.write("c"); }
    { QFile f(dir + "/model-x/tokenizer/vocab.json"); ASSERT_TRUE(f.open(QIODevice::WriteOnly)); f.write("{}"); }

    EXPECT_FALSE(ExtractTool::flattenWrapper(dir, "other-name")) << "legit single sub-directory layouts must be left alone";
    EXPECT_TRUE(QFileInfo::exists(dir + "/model-x/conv_frontend.onnx"));

    EXPECT_TRUE(ExtractTool::flattenWrapper(dir, "model-x"));
    EXPECT_TRUE(QFileInfo::exists(dir + "/conv_frontend.onnx"));
    EXPECT_TRUE(QFileInfo::exists(dir + "/tokenizer/vocab.json"));
    EXPECT_FALSE(QFileInfo::exists(dir + "/model-x"));
    EXPECT_FALSE(ExtractTool::flattenWrapper(dir, "model-x")) << "already flat: nothing to do";
}
