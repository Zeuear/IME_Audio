#include <gtest/gtest.h>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTemporaryDir>

#include "ConfigManager.h"
#include "widgets/SettingsBinder.h"

namespace {
// 控件需要 QApplication；其它测试文件按需创建的是 QCoreApplication，
// 用全局环境在所有测试之前建好，它们看到 qApp 已存在就会直接复用
class WidgetAppEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        if (!qApp) {
            static int argc = 0;
            static char* argv[] = { nullptr };
            new QApplication(argc, argv);
        }
    }
};
const auto* const kWidgetApp = ::testing::AddGlobalTestEnvironment(new WidgetAppEnvironment);

void ensureApp()
{
    ASSERT_NE(qobject_cast<QApplication*>(qApp), nullptr);
}
} // namespace

// 设置改了就立即保存：ConfigManager 漏读或漏写任何一个用户可改的字段，重启后设置就丢了。
// 每个字段设成非默认值，保存再读回逐个比较；新增设置时在这里补一行。
TEST(ConfigRoundTrip, EveryUserSettingSurvivesSaveAndLoad) {
    QTemporaryDir dir;
    const QString oldPath = ConfigManager::configFilePath();
    ConfigManager::setConfigFilePath(dir.filePath("test.ini"));

    ConfigManager writer;
    AppConfig& w = writer.config();
    w.backend = AsrBackendKind::Groq;
    w.groqKey = "groq"; w.gladiaKey = "gladia"; w.geminiKey = "gemini";
    w.continuousMode = true;
    w.liveCaptionEnabled = false; w.liveCaptionFontSize = 21; w.liveCaptionAtTop = true;
    w.hotkey = "Ctrl+Shift+K"; w.hotkeyHold = true;
    w.audio.deviceId = 3; w.audio.deviceName = "mic"; w.audio.outputDeviceName = "spk";
    w.audio.voiceThreshold = 901; w.audio.silenceTimeoutMs = 1234; w.audio.minRecordMs = 456; w.audio.maxRecordMs = 7890;
    w.sherpa.useGpu = true; w.sherpa.threads = 7; w.sherpa.languageModel = "Spanish"; w.sherpa.localModelRepoId = "a/b";
    w.polish.aiEngineIndex = 2; w.polish.enableAiEnhancement = true; w.polish.apiKey = "k"; w.polish.apiUrl = "u";
    w.polish.model = "m"; w.polish.prompt = "p"; w.polish.aiStyle = "s"; w.polish.targetLang = "t";
    ASSERT_TRUE(writer.save());

    ConfigManager reader;
    reader.load();
    const AppConfig& r = reader.config();
    EXPECT_EQ(r.backend, AsrBackendKind::Groq);
    EXPECT_EQ(r.groqKey, "groq"); EXPECT_EQ(r.gladiaKey, "gladia"); EXPECT_EQ(r.geminiKey, "gemini");
    EXPECT_TRUE(r.continuousMode.load());
    EXPECT_FALSE(r.liveCaptionEnabled.load()); EXPECT_EQ(r.liveCaptionFontSize, 21); EXPECT_TRUE(r.liveCaptionAtTop);
    EXPECT_EQ(r.hotkey, "Ctrl+Shift+K"); EXPECT_TRUE(r.hotkeyHold);
    EXPECT_EQ(r.audio.deviceId, 3); EXPECT_EQ(r.audio.deviceName, "mic"); EXPECT_EQ(r.audio.outputDeviceName, "spk");
    EXPECT_EQ(r.audio.voiceThreshold, 901); EXPECT_EQ(r.audio.silenceTimeoutMs, 1234);
    EXPECT_EQ(r.audio.minRecordMs, 456); EXPECT_EQ(r.audio.maxRecordMs, 7890);
    EXPECT_TRUE(r.sherpa.useGpu); EXPECT_EQ(r.sherpa.threads, 7);
    EXPECT_EQ(r.sherpa.languageModel, "Spanish"); EXPECT_EQ(r.sherpa.localModelRepoId, "a/b");
    EXPECT_EQ(r.polish.aiEngineIndex, 2); EXPECT_TRUE(r.polish.enableAiEnhancement);
    EXPECT_EQ(r.polish.apiKey, "k"); EXPECT_EQ(r.polish.apiUrl, "u"); EXPECT_EQ(r.polish.model, "m");
    EXPECT_EQ(r.polish.prompt, "p"); EXPECT_EQ(r.polish.aiStyle, "s"); EXPECT_EQ(r.polish.targetLang, "t");

    // 赋值运算符同样要拷贝全部字段
    AppConfig copy;
    copy = r;
    EXPECT_EQ(copy.geminiKey, "gemini"); EXPECT_TRUE(copy.hotkeyHold); EXPECT_EQ(copy.liveCaptionFontSize, 21);

    ConfigManager::setConfigFilePath(oldPath);
}

class SettingsBinderTest : public ::testing::Test {
protected:
    void SetUp() override { ensureApp(); }
    AppConfig cfg;
    SettingsBinder binder{ cfg };
    QList<unsigned> changes;
    void track() { QObject::connect(&binder, &SettingsBinder::changed, [this](unsigned e) { changes << e; }); }
};

TEST_F(SettingsBinderTest, LoadFillsWidgetsWithoutWritingBack) {
    QSpinBox spin;
    spin.setRange(0, 10000);
    cfg.audio.voiceThreshold = 777;
    binder.bind(&spin, [](const AppConfig& c) { return c.audio.voiceThreshold; },
                [](AppConfig& c, int v) { c.audio.voiceThreshold = v; }, {});
    track();
    binder.loadAll();
    EXPECT_EQ(spin.value(), 777);
    EXPECT_TRUE(changes.isEmpty());
}

TEST_F(SettingsBinderTest, InstantWidgetWritesAndReportsEffects) {
    QCheckBox box;
    binder.bind(&box, [](const AppConfig& c) { return c.liveCaptionEnabled.load(); },
                [](AppConfig& c, bool v) { c.liveCaptionEnabled = v; },
                { SettingsBinder::Apply::Instant, SettingsBinder::LiveCaption });
    binder.loadAll();
    track();
    box.setChecked(!box.isChecked());
    EXPECT_EQ(cfg.liveCaptionEnabled.load(), box.isChecked());
    ASSERT_EQ(changes.size(), 1);
    EXPECT_EQ(changes[0], unsigned(SettingsBinder::LiveCaption));
}

TEST_F(SettingsBinderTest, OnCommitTextWaitsForEditingFinishedAndTrims) {
    QLineEdit edit;
    binder.bind(&edit, [](const AppConfig& c) { return c.groqKey; },
                [](AppConfig& c, const QString& v) { c.groqKey = v; },
                { SettingsBinder::Apply::OnCommit });
    track();
    edit.setText("  abc ");
    EXPECT_TRUE(cfg.groqKey.isEmpty());   // 输到一半不生效
    emit edit.editingFinished();
    EXPECT_EQ(cfg.groqKey, "abc");
    emit edit.editingFinished();          // 没改动再离开焦点，不重复通知
    EXPECT_EQ(changes.size(), 1);
}

TEST_F(SettingsBinderTest, ComboIndexOutOfRangeKeepsCurrent) {
    QComboBox combo;
    combo.addItems({ "a", "b" });
    cfg.polish.aiEngineIndex = 5;
    binder.bindIndex(&combo, [](const AppConfig& c) { return c.polish.aiEngineIndex; },
                     [](AppConfig& c, int v) { c.polish.aiEngineIndex = v; }, {});
    binder.loadAll();
    EXPECT_EQ(combo.currentIndex(), 0);
}

TEST_F(SettingsBinderTest, OnlyMarkedWidgetsLockWhileDictating) {
    QCheckBox locked, free;
    binder.bind(&locked, [](const AppConfig& c) { return c.continuousMode.load(); },
                [](AppConfig& c, bool v) { c.continuousMode = v; },
                { SettingsBinder::Apply::Instant, SettingsBinder::NoEffect, true });
    binder.bind(&free, [](const AppConfig& c) { return c.liveCaptionAtTop; },
                [](AppConfig& c, bool v) { c.liveCaptionAtTop = v; }, {});
    binder.setDictating(true);
    EXPECT_FALSE(locked.isEnabled());
    EXPECT_TRUE(free.isEnabled());
    binder.setDictating(false);
    EXPECT_TRUE(locked.isEnabled());
}
