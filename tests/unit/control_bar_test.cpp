#include <gtest/gtest.h>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <optional>

#include "services/WorkflowManager.h"
#include "ui/controls/ControlBarState.h"
#include "ui/controls/FloatingWindow.h"

// ---- 按钮状态：开始键只在空闲时可点，停止键在有任务进行时可点 ----

TEST(ControlBarState, IdleCanOnlyStart) {
    const auto s = ControlBarState::forWorkflow(WorkflowState::Idle);
    EXPECT_TRUE(s.startEnabled);
    EXPECT_FALSE(s.stopEnabled);
}

TEST(ControlBarState, LoadingAndRecordingAndTranscribingCanOnlyStop) {
    for (auto st : { WorkflowState::Loading, WorkflowState::Recording,
                     WorkflowState::Transcribing, WorkflowState::Processing }) {
        const auto s = ControlBarState::forWorkflow(st);
        EXPECT_FALSE(s.startEnabled) << static_cast<int>(st);
        EXPECT_TRUE(s.stopEnabled) << static_cast<int>(st);
    }
}

TEST(ControlBarState, StoppingAndErrorDisableBothButtons) {
    for (auto st : { WorkflowState::Stopping, WorkflowState::Error }) {
        const auto s = ControlBarState::forWorkflow(st);
        EXPECT_FALSE(s.startEnabled) << static_cast<int>(st);
        EXPECT_FALSE(s.stopEnabled) << static_cast<int>(st);
    }
}

// ---- 位置：保存的位置要落在仍然存在的屏幕里 ----

namespace {
const QRect kMain(0, 0, 1920, 1040);          // 主屏可用区域
const QRect kRight(1920, 0, 1280, 1000);      // 右侧副屏
const QSize kBar(300, 48);
} // namespace

TEST(FloatingWindowPlace, NoSavedPositionGoesTopCenterOfPrimary) {
    const QPoint p = FloatingWindow::place(std::nullopt, kBar, { kMain }, kMain);
    EXPECT_EQ(p.x(), (1920 - 300) / 2);
    EXPECT_EQ(p.y(), 12);
}

TEST(FloatingWindowPlace, SavedPositionInsideAScreenIsKept) {
    const QPoint p = FloatingWindow::place(QPoint(2100, 300), kBar, { kMain, kRight }, kMain);
    EXPECT_EQ(p, QPoint(2100, 300));
}

TEST(FloatingWindowPlace, SavedOnAnUnpluggedScreenFallsBackToPrimaryTopCenter) {
    // 之前放在右侧副屏，现在只剩主屏
    const QPoint p = FloatingWindow::place(QPoint(2100, 300), kBar, { kMain }, kMain);
    EXPECT_EQ(p.x(), (1920 - 300) / 2);
    EXPECT_EQ(p.y(), 12);
}

TEST(FloatingWindowPlace, PartlyOffScreenIsPulledBackInside) {
    // 右边缘超出主屏 100 像素
    const QPoint p = FloatingWindow::place(QPoint(1920 - 300 + 100, 20), kBar, { kMain }, kMain);
    EXPECT_EQ(p, QPoint(1920 - 300, 20));
}

TEST(FloatingWindowPlace, TooHighOrTooLowIsPulledBackInside) {
    // 上边露出 30 像素在屏幕外；下边只剩 10 像素在屏幕内（中心都已落到屏幕外）
    EXPECT_EQ(FloatingWindow::place(QPoint(100, -30), kBar, { kMain }, kMain), QPoint(100, 0));
    EXPECT_EQ(FloatingWindow::place(QPoint(100, 1030), kBar, { kMain }, kMain), QPoint(100, 1040 - 48));
}

TEST(FloatingWindowPlace, CompletelyOffEveryScreenFallsBackToPrimaryTopCenter) {
    // 分辨率变小或显示器拔掉后，保存的位置整个落在所有屏幕之外
    const QPoint p = FloatingWindow::place(QPoint(100, 5000), kBar, { kMain }, kMain);
    EXPECT_EQ(p.x(), (1920 - 300) / 2);
    EXPECT_EQ(p.y(), 12);
}

TEST(FloatingWindowPlace, UsesTheScreenThatHoldsTheWindowCenter) {
    // 窗口大半在副屏、一小截压在主屏边缘：以中心所在屏幕为准，整个收进副屏，不被拽回主屏
    const QPoint p = FloatingWindow::place(QPoint(1920 - 20, 100), kBar, { kMain, kRight }, kMain);
    EXPECT_EQ(p, QPoint(1920, 100));
}

// ---- 横条的设置入口与右键菜单：只测信号，布局和外观靠运行确认 ----

#include <QAbstractButton>
#include <QAction>
#include "ui/controls/ControlBar.h"

namespace {
struct BarSignals {
    explicit BarSignals(ControlBar& bar) {
        QObject::connect(&bar, &ControlBar::settingsRequested, &bar, [this]() { ++settings; });
        QObject::connect(&bar, &ControlBar::quitRequested, &bar, [this]() { ++quit; });
    }
    int settings = 0;
    int quit = 0;
};
} // namespace

TEST(ControlBarEntry, SettingsButtonAsksForTheSettingsWindow) {
    ControlBar bar;
    BarSignals s(bar);
    ASSERT_NE(bar.settingsButton(), nullptr);
    bar.settingsButton()->click();
    EXPECT_EQ(s.settings, 1);
    EXPECT_EQ(s.quit, 0);
}

TEST(ControlBarEntry, ContextMenuOffersSettingsAndQuit) {
    ControlBar bar;
    BarSignals s(bar);
    ASSERT_NE(bar.settingsAction(), nullptr);
    ASSERT_NE(bar.quitAction(), nullptr);

    bar.settingsAction()->trigger();
    EXPECT_EQ(s.settings, 1);
    EXPECT_EQ(s.quit, 0);

    bar.quitAction()->trigger();
    EXPECT_EQ(s.quit, 1);
    EXPECT_EQ(s.settings, 1);
}

TEST(ControlBarEntry, SettingsButtonStaysUsableWhileDictating) {
    // 录音中也要能打开设置（听写时设置页自己会锁住不该改的项，入口不能消失）
    ControlBar bar;
    for (auto st : { WorkflowState::Idle, WorkflowState::Loading, WorkflowState::Recording,
                     WorkflowState::Transcribing, WorkflowState::Stopping, WorkflowState::Error }) {
        bar.setWorkflowState(st);
        EXPECT_TRUE(bar.settingsButton()->isEnabled()) << static_cast<int>(st);
    }
}

// ---- 转录内容图标：开关式，窗口开着时高亮；宿主改状态不能再触发一次开关 ----

TEST(ControlBarTranscript, ClickingTheIconAsksToOpenThenClose) {
    ControlBar bar;
    QList<bool> asked;
    QObject::connect(&bar, &ControlBar::transcriptToggled, &bar, [&](bool open) { asked << open; });
    ASSERT_NE(bar.transcriptButton(), nullptr);

    bar.transcriptButton()->click();
    bar.transcriptButton()->click();
    EXPECT_EQ(asked, (QList<bool>{ true, false }));
}

TEST(ControlBarTranscript, HostClosingTheWindowResetsTheIconWithoutAskingAgain) {
    ControlBar bar;
    int emitted = 0;
    QObject::connect(&bar, &ControlBar::transcriptToggled, &bar, [&](bool) { ++emitted; });

    bar.transcriptButton()->click();          // 用户点开
    ASSERT_EQ(emitted, 1);
    EXPECT_TRUE(bar.transcriptButton()->isChecked());

    bar.setTranscriptOpen(false);             // 用户点了窗口右上角 ×，宿主通知横条
    EXPECT_FALSE(bar.transcriptButton()->isChecked());
    EXPECT_EQ(emitted, 1);                    // 不再触发一次"关闭"
}

TEST(ControlBarTranscript, IconStaysUsableWhileDictating) {
    ControlBar bar;
    for (auto st : { WorkflowState::Idle, WorkflowState::Recording, WorkflowState::Stopping, WorkflowState::Error }) {
        bar.setWorkflowState(st);
        EXPECT_TRUE(bar.transcriptButton()->isEnabled()) << static_cast<int>(st);
    }
}

// ---- 位置和大小的保存读取：横条、稿子窗口各一个分组，互不覆盖，也不动 ConfigManager 的字段 ----

#include <QTemporaryDir>
#include "config/ConfigManager.h"

namespace {
class TempConfigFile {
public:
    TempConfigFile() : m_old(ConfigManager::configFilePath()) {
        ConfigManager::setConfigFilePath(m_dir.filePath("test.ini"));
    }
    ~TempConfigFile() { ConfigManager::setConfigFilePath(m_old); }
private:
    QTemporaryDir m_dir;
    QString m_old;
};
} // namespace

TEST(FloatingWindowStore, PositionAndSizeRoundTrip) {
    TempConfigFile cfg;
    FloatingWindow::savePosition("Win", QPoint(120, -30));
    FloatingWindow::saveSize("Win", QSize(640, 480));
    EXPECT_EQ(FloatingWindow::loadPosition("Win"), std::optional<QPoint>(QPoint(120, -30)));
    EXPECT_EQ(FloatingWindow::loadSize("Win"), std::optional<QSize>(QSize(640, 480)));
}

TEST(FloatingWindowStore, NothingSavedMeansNothingLoaded) {
    TempConfigFile cfg;
    EXPECT_FALSE(FloatingWindow::loadPosition("Win").has_value());
    EXPECT_FALSE(FloatingWindow::loadSize("Win").has_value());
}

TEST(FloatingWindowStore, GroupsDoNotOverwriteEachOther) {
    TempConfigFile cfg;
    FloatingWindow::savePosition("ControlBar", QPoint(1, 2));
    FloatingWindow::savePosition("TranscriptWindow", QPoint(30, 40));
    EXPECT_EQ(FloatingWindow::loadPosition("ControlBar"), std::optional<QPoint>(QPoint(1, 2)));
    EXPECT_EQ(FloatingWindow::loadPosition("TranscriptWindow"), std::optional<QPoint>(QPoint(30, 40)));
}

TEST(FloatingWindowStore, SharesTheConfigFileWithoutDamagingEitherSide) {
    TempConfigFile cfg;
    {
        ConfigManager writer;
        writer.config().hotkey = "Ctrl+Shift+K";
        ASSERT_TRUE(writer.save());
    }
    FloatingWindow::savePosition("ControlBar", QPoint(7, 8));
    {
        // 设置再保存一次：不能把位置分组冲掉
        ConfigManager again;
        again.load();
        EXPECT_EQ(again.config().hotkey, "Ctrl+Shift+K");
        ASSERT_TRUE(again.save());
    }
    EXPECT_EQ(FloatingWindow::loadPosition("ControlBar"), std::optional<QPoint>(QPoint(7, 8)));
}


// ---- 录音计时与会话状态 ----

TEST(ControlBarState, SessionIsActiveFromLoadingUntilItReturnsToIdle) {
    for (auto st : { WorkflowState::Loading, WorkflowState::Recording, WorkflowState::Transcribing,
                     WorkflowState::Processing, WorkflowState::Stopping }) {
        EXPECT_TRUE(ControlBarState::isSessionActive(st)) << static_cast<int>(st);
    }
    EXPECT_FALSE(ControlBarState::isSessionActive(WorkflowState::Idle));
    EXPECT_FALSE(ControlBarState::isSessionActive(WorkflowState::Error));
}

TEST(ControlBarState, ElapsedTimeUnderAnHourIsMinutesAndSeconds) {
    EXPECT_EQ(ControlBarState::formatElapsed(0), "00:00");
    EXPECT_EQ(ControlBarState::formatElapsed(9), "00:09");
    EXPECT_EQ(ControlBarState::formatElapsed(59), "00:59");
    EXPECT_EQ(ControlBarState::formatElapsed(60), "01:00");
    EXPECT_EQ(ControlBarState::formatElapsed(754), "12:34");
    EXPECT_EQ(ControlBarState::formatElapsed(3599), "59:59");
}

TEST(ControlBarState, ElapsedTimeFromAnHourOnShowsHours) {
    EXPECT_EQ(ControlBarState::formatElapsed(3600), "1:00:00");
    EXPECT_EQ(ControlBarState::formatElapsed(3725), "1:02:05");
    EXPECT_EQ(ControlBarState::formatElapsed(36000 + 61), "10:01:01");
}

TEST(ControlBarState, NegativeElapsedTimeIsZero) {
    EXPECT_EQ(ControlBarState::formatElapsed(-5), "00:00");
}

TEST(ControlBarTimer, RunsOnlyWhileASessionIsActiveAndResetsAfterwards) {
    ControlBar bar;
    EXPECT_FALSE(bar.timerRunning());
    EXPECT_EQ(bar.elapsedText(), "00:00");

    bar.setWorkflowState(WorkflowState::Loading);
    EXPECT_TRUE(bar.timerRunning());
    bar.setWorkflowState(WorkflowState::Recording);
    EXPECT_TRUE(bar.timerRunning());
    bar.setWorkflowState(WorkflowState::Transcribing);   // 连续听写在录音和转录之间来回切，计时不能被打断
    EXPECT_TRUE(bar.timerRunning());

    bar.setWorkflowState(WorkflowState::Idle);
    EXPECT_FALSE(bar.timerRunning());
    EXPECT_EQ(bar.elapsedText(), "00:00");
}

TEST(ControlBarTimer, ErrorStopsTheTimer) {
    ControlBar bar;
    bar.setWorkflowState(WorkflowState::Recording);
    bar.setWorkflowState(WorkflowState::Error);
    EXPECT_FALSE(bar.timerRunning());
}

TEST(ControlBarMenu, AlwaysOnTopIsACheckableMenuItemThatTellsTheHost) {
    ControlBar bar;
    QAction* a = bar.alwaysOnTopAction();
    ASSERT_NE(a, nullptr);
    EXPECT_TRUE(a->isCheckable());

    int fired = 0;
    bool last = true;
    QObject::connect(&bar, &ControlBar::alwaysOnTopToggled, &bar, [&](bool on) { ++fired; last = on; });
    a->trigger();
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(last, !a->isChecked() ? false : true);
}

TEST(ControlBarMenu, SettingAlwaysOnTopFromTheHostOnlyMovesTheCheckmark) {
    ControlBar bar;
    int fired = 0;
    QObject::connect(&bar, &ControlBar::alwaysOnTopToggled, &bar, [&](bool) { ++fired; });
    bar.setAlwaysOnTop(false);
    EXPECT_FALSE(bar.alwaysOnTopAction()->isChecked());
    bar.setAlwaysOnTop(true);
    EXPECT_TRUE(bar.alwaysOnTopAction()->isChecked());
    EXPECT_EQ(fired, 0);
}
