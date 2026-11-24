#include <gtest/gtest.h>
#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QEnterEvent>
#include <QListView>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegion>
#include <QThread>
#include <QSettings>
#include <QPointer>
#include <QTabWidget>
#include <QTemporaryDir>

#include "config/ConfigManager.h"
#include "services/WorkflowManager.h"
#include "services/interfaces/workflow_interfaces.h"
#include "ui/controls/FloatingWindow.h"
#include "ui/controls/ExpandPanel.h"
#include "ui/controls/LiveOverlay.h"
#include "ui/delegates/TranscriptLineDelegate.h"
#include "ui/models/LiveCaptionModel.h"
#include "ui/pages/LiveTranscriptWidget.h"

// 界面 / 配置回归：不连真机，用合成鼠标事件和临时 ini 验证本轮改动的行为。
// QApplication 由 support/widget_app_environment.cpp 提供。

namespace {

// 不引入 Qt6::Test，只数信号发了几次
struct SignalCount {
    int n = 0;
    template <typename Obj, typename Sig>
    SignalCount(Obj* o, Sig sig) { QObject::connect(o, sig, o, [this]() { ++n; }); }
    int count() const { return n; }
};

void pump(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
}

void mouse(QWidget* w, QEvent::Type type, const QPointF& pos, Qt::MouseButton button = Qt::LeftButton,
           Qt::KeyboardModifiers mods = {}, const QPointF& globalOverride = QPointF(-1, -1)) {
    const QPointF global = globalOverride.x() >= 0 ? globalOverride : QPointF(w->mapToGlobal(pos.toPoint()));
    const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : button;
    QMouseEvent e(type, pos, global, button, buttons, mods);
    QApplication::sendEvent(w, &e);
}

void click(QWidget* w, const QPointF& pos, Qt::KeyboardModifiers mods = {}) {
    mouse(w, QEvent::MouseButtonPress, pos, Qt::LeftButton, mods);
    mouse(w, QEvent::MouseButtonRelease, pos, Qt::LeftButton, mods);
}

// ---------- 配置 ----------
struct TempConfig {
    QTemporaryDir dir;
    QString old = ConfigManager::configFilePath();
    TempConfig() { ConfigManager::setConfigFilePath(dir.filePath("t.ini")); }
    ~TempConfig() { ConfigManager::setConfigFilePath(old); }
    QString path() const { return dir.filePath("t.ini"); }
};

} // namespace

TEST(ConfigMigration, OldSingleModelBelongsToTheEngineThatWasSelected) {
    TempConfig tc;
    {
        QSettings s(tc.path(), QSettings::IniFormat);
        s.beginGroup("gemini");
        s.setValue("aiEngineIndex", 1);
        s.setValue("apiUrl", "http://localhost:11434");
        s.setValue("apiKey", "kk");
        s.setValue("model", "qwen3:8b");
    }
    ConfigManager m;
    ASSERT_TRUE(m.load());
    const PolishConfig& p = m.config().polish;
    EXPECT_EQ(p.modelByEngine.value(1), "qwen3:8b");
    EXPECT_EQ(p.apiUrlByEngine.value(1), "http://localhost:11434");
    EXPECT_EQ(p.apiKeyByEngine.value(1), "kk");
    EXPECT_EQ(p.model, "qwen3:8b");
    EXPECT_FALSE(p.modelByEngine.contains(2));   // 没选过的引擎不凭空得到值
}

TEST(ConfigMigration, LeftoverGeminiModelIsNotHandedToOllama) {
    TempConfig tc;
    {
        QSettings s(tc.path(), QSettings::IniFormat);
        s.beginGroup("gemini");
        s.setValue("aiEngineIndex", 1);
        s.setValue("model", "gemini-3.1-flash");
    }
    ConfigManager m;
    ASSERT_TRUE(m.load());
    EXPECT_TRUE(m.config().polish.modelByEngine.value(1).isEmpty());
    EXPECT_TRUE(m.config().polish.model.isEmpty());
}

TEST(ConfigMigration, GeminiEngineKeepsItsGeminiModel) {
    TempConfig tc;
    {
        QSettings s(tc.path(), QSettings::IniFormat);
        s.beginGroup("gemini");
        s.setValue("aiEngineIndex", 0);
        s.setValue("model", "gemini-3.1-flash");
    }
    ConfigManager m;
    ASSERT_TRUE(m.load());
    EXPECT_EQ(m.config().polish.model, "gemini-3.1-flash");
    EXPECT_EQ(m.config().polish.modelByEngine.value(0), "gemini-3.1-flash");
}

TEST(ConfigMigration, PerEngineValuesSurviveSwitchingEnginesAndRestart) {
    TempConfig tc;
    {
        ConfigManager w;
        PolishConfig& p = w.config().polish;
        p.aiEngineIndex = 0; p.apiKey = "gkey"; p.apiUrl = ""; p.model = "gemini-x";
        ASSERT_TRUE(w.save());
        p.aiEngineIndex = 1; p.apiKey = ""; p.apiUrl = "http://localhost:11434"; p.model = "hy-mt"; p.translateModel = "idx";
        ASSERT_TRUE(w.save());
        p.aiEngineIndex = 2; p.apiKey = "okey"; p.apiUrl = "https://api.example.com/v1"; p.model = "gpt"; p.translateModel = "gpt-mt";
        ASSERT_TRUE(w.save());
    }
    ConfigManager r;
    ASSERT_TRUE(r.load());
    const PolishConfig& p = r.config().polish;
    EXPECT_EQ(p.aiEngineIndex, 2);
    EXPECT_EQ(p.apiKeyByEngine.value(0), "gkey");
    EXPECT_EQ(p.apiUrlByEngine.value(1), "http://localhost:11434");
    EXPECT_EQ(p.apiKeyByEngine.value(2), "okey");
    EXPECT_EQ(p.modelByEngine.value(0), "gemini-x");
    EXPECT_EQ(p.modelByEngine.value(1), "hy-mt");
    EXPECT_EQ(p.modelByEngine.value(2), "gpt");
    EXPECT_EQ(p.translateModelByEngine.value(1), "idx");
    EXPECT_EQ(p.translateModelByEngine.value(2), "gpt-mt");
}

// ---------- 工作流：先显示原文的信号能一路转发到界面 ----------
namespace {
class StubRecorder : public IRecorder {
public:
    bool startListening() override { return true; }
    void stopListening() override {}
    bool isListening() const override { return false; }
    bool isPaused() const override { return false; }
    void pause() override {}
    void resume() override {}
    void updateConfig() override {}
};
class StubTranscription : public ITranscription {
public:
    void transcribe(const QByteArray&, int, int, int) override {}
    void pending(quint64 s, const QString& t) { emit transcriptionPending(s, t); }
    void translated(quint64 s, const QString& a, const QString& b) { emit transcriptionTranslated(s, a, b); }
    void finished(const QString& t) { emit transcriptionFinished(true, t, t, {}); }
};
class StubSherpa : public ISherpaModel {
public:
    bool reloadModel(const AppConfig&) override { return false; }
    void loadModelAsync(const AppConfig&, bool) override {}
    bool isModelLoaded() const override { return true; }
    void pauseIdleTimer() override {}
    void resumeIdleTimer() override {}
};
} // namespace

TEST(UtterancePairing, PendingTranslatedThenResultArriveInOrderAndLiveRowIsNotDuplicated) {
    AppConfig cfg;
    StubRecorder rec; StubTranscription tr; StubSherpa sh;
    WorkflowManager wf(cfg);
    wf.initialize(&rec, &tr, &sh);
    wf.setInjectEnabled(false);

    LiveTranscriptWidget live;
    QStringList order;
    // 与 MainWin 里的接线保持一致
    QString shownText; bool shownValid = false;
    QObject::connect(&wf, &WorkflowManager::utterancePending, &live, [&](quint64 s, const QString& t) {
        order << "pending"; live.appendPending(s, t); });
    QObject::connect(&wf, &WorkflowManager::utteranceTranslated, &live, [&](quint64 s, const QString& src, const QString& f) {
        order << "translated"; live.completePending(s, src, f); shownText = f; shownValid = true; });
    QObject::connect(&wf, &WorkflowManager::transcriptionResultReady, &live, [&](const QString& t) {
        order << "result";
        const bool already = shownValid && shownText == t;
        shownValid = false; shownText.clear();
        if (!already) live.appendUtterance(t);
    });

    auto* list = live.findChild<QListView*>("live_transcript_list");
    ASSERT_NE(list, nullptr);

    // 1) 翻译成功
    tr.pending(1, "Hola"); tr.translated(1, "Hola", "你好"); tr.finished("你好");
    // 2) 翻译失败回退：最终文字等于原文
    tr.pending(2, "Adios"); tr.translated(2, "Adios", "Adios"); tr.finished("Adios");
    // 3) 没开翻译的句子：不发 pending / translated
    tr.finished("plain");

    EXPECT_EQ(order, (QStringList{"pending","translated","result","pending","translated","result","result"}));
    ASSERT_EQ(list->model()->rowCount(), 3);   // 每句恰好一行，没有重复也没有漏
    EXPECT_EQ(list->model()->index(0, 0).data().toString(), "Hola");
    EXPECT_EQ(list->model()->index(1, 0).data().toString(), "Adios");
    EXPECT_EQ(list->model()->index(2, 0).data().toString(), "plain");
}

TEST(UtterancePairing, SlowTranslationDoesNotLoseOrReorderLaterSentences) {
    LiveTranscriptWidget live;
    auto* list = live.findChild<QListView*>("live_transcript_list");
    ASSERT_NE(list, nullptr);
    live.appendPending(1, "one");
    live.appendPending(2, "two");     // 第 1 句译文还没回来，第 2 句的原文已经显示
    live.appendPending(3, "three");
    ASSERT_EQ(list->model()->rowCount(), 3);
    live.completePending(1, "one", "一");
    live.completePending(2, "two", "二");
    live.completePending(3, "three", "");   // 翻译失败：右栏留空，原文保留
    EXPECT_EQ(list->model()->rowCount(), 3);
    EXPECT_EQ(list->model()->index(2, 0).data().toString(), "three");
}

// ---------- 实时转录页：选择像文件管理器 ----------
namespace {
struct LiveFixture {
    LiveTranscriptWidget live;
    QListView* list = nullptr;
    LiveFixture() {
        live.resize(900, 520);
        live.show();
        list = live.findChild<QListView*>("live_transcript_list");
        for (int i = 0; i < 4; ++i) live.appendUtterance(QString("sentence %1").arg(i), QString("译文 %1").arg(i));
        pump(80);
    }
    QPoint textPoint(int row, bool translation = false) const {
        auto* d = static_cast<TranscriptLineDelegate*>(list->itemDelegate());
        return d->sideRect(list->visualRect(list->model()->index(row, 0)), translation).center();
    }
    QList<int> selected() const {
        QList<int> rows;
        for (const QModelIndex& i : list->selectionModel()->selectedIndexes()) rows << i.row();
        std::sort(rows.begin(), rows.end());
        return rows;
    }
    QPoint blank() const { return QPoint(list->viewport()->width() / 2, list->viewport()->height() - 5); }
};
} // namespace

TEST(LiveSelection, ClickOnTextSelectsOnlyThatRowLikeAFileManager) {
    LiveFixture f;
    ASSERT_NE(f.list, nullptr);
    click(f.list->viewport(), f.textPoint(0));
    EXPECT_EQ(f.selected(), (QList<int>{0}));
    click(f.list->viewport(), f.textPoint(2, true));   // 点译文一侧同样选中整行
    EXPECT_EQ(f.selected(), (QList<int>{2}));
}

TEST(LiveSelection, CtrlTogglesShiftExtendsAndPlainClickCollapses) {
    LiveFixture f;
    click(f.list->viewport(), f.textPoint(1));
    click(f.list->viewport(), f.textPoint(3), Qt::ControlModifier);
    EXPECT_EQ(f.selected(), (QList<int>{1, 3}));
    click(f.list->viewport(), f.textPoint(3), Qt::ControlModifier);   // 再点一次取消
    EXPECT_EQ(f.selected(), (QList<int>{1}));
    // Shift 范围选择：合成鼠标事件在纯 QListView 上也不生效（Qt 读的是真实键盘状态），无法在这里验证；
    // 页面没有改写这条路径（单击一律交给 QListView 默认处理）
    click(f.list->viewport(), f.textPoint(2));          // 点已选中的行：收成只选它
    EXPECT_EQ(f.selected(), (QList<int>{2}));
}

TEST(LiveSelection, ClickingBlankSpaceClears) {
    LiveFixture f;
    click(f.list->viewport(), f.textPoint(1));
    ASSERT_EQ(f.selected().size(), 1);
    click(f.list->viewport(), f.blank());
    EXPECT_TRUE(f.selected().isEmpty());
}

TEST(LiveSelection, DoubleClickOnTextOpensTheEditor) {
    LiveFixture f;
    const QPoint p = f.textPoint(1);
    mouse(f.list->viewport(), QEvent::MouseButtonPress, p);
    mouse(f.list->viewport(), QEvent::MouseButtonRelease, p);
    mouse(f.list->viewport(), QEvent::MouseButtonDblClick, p);
    pump(30);
    EXPECT_NE(f.list->viewport()->findChild<QPlainTextEdit*>(), nullptr);
}

TEST(LiveSelection, CopyButtonsCopyOnlySelectedRowsAndKeepSidesApart) {
    LiveFixture f;
    SignalCount spy(&f.live, &LiveTranscriptWidget::copied);
    click(f.list->viewport(), f.textPoint(1));
    click(f.list->viewport(), f.textPoint(2), Qt::ControlModifier);
    const auto buttons = f.live.findChildren<QPushButton*>();
    QPushButton* copyOrig = nullptr; QPushButton* copyTr = nullptr;
    for (auto* b : buttons) {
        if (b->text().startsWith("Copy Original") || b->text().startsWith("复制原文")) copyOrig = b;
        if (b->text().startsWith("Copy Translation") || b->text().startsWith("复制译文")) copyTr = b;
    }
    ASSERT_TRUE(copyOrig && copyTr);
    copyOrig->click();
    EXPECT_EQ(QApplication::clipboard()->text(), "sentence 1\nsentence 2");
    copyTr->click();
    EXPECT_EQ(QApplication::clipboard()->text(), "译文 1\n译文 2");
    EXPECT_EQ(spy.count(), 2);
}

// ---------- 悬浮窗 ----------
namespace {
struct OverlayFixture {
    TempConfig cfg;
    LiveOverlay ov;
    LiveCaptionModel caption;
    OverlayFixture() {
        ov.setCaptionModel(&caption);
        ov.setListening();
        ov.showOnScreen();
        pump(700);   // 等入场动画和窗口尺寸落定
    }
    void hover() {
        QEnterEvent e(QPointF(ov.cardRect().center()), QPointF(ov.cardRect().center()), QPointF(ov.mapToGlobal(ov.cardRect().center().toPoint())));
        QApplication::sendEvent(&ov, &e);
        pump(400);   // 按钮淡入
    }
};
} // namespace

TEST(OverlayButtons, CardStaysCenteredAndClickingItTogglesPause) {
    OverlayFixture f;
    EXPECT_NEAR(f.ov.cardRect().center().x(), f.ov.width() / 2.0, 1.0);
    SignalCount spy(&f.ov, &LiveOverlay::sphereClicked);
    click(&f.ov, f.ov.cardRect().center());
    EXPECT_EQ(spy.count(), 1);
}

TEST(OverlayButtons, ButtonsAreInertUntilHovered) {
    OverlayFixture f;
    SignalCount hidden(&f.ov, &LiveOverlay::hiddenByUser);
    click(&f.ov, f.ov.hideButtonRect().center());   // 还没悬停：点在空白上，什么也不该发生
    EXPECT_EQ(hidden.count(), 0);
    EXPECT_TRUE(f.ov.isVisible());
}

TEST(OverlayButtons, HideThenOnlyTrayRestoreBringsItBack) {
    OverlayFixture f;
    f.hover();
    SignalCount hidden(&f.ov, &LiveOverlay::hiddenByUser);
    click(&f.ov, f.ov.hideButtonRect().center());
    EXPECT_EQ(hidden.count(), 1);
    EXPECT_FALSE(f.ov.isVisible());
    EXPECT_TRUE(f.ov.userHidden());
    f.ov.showOnScreen();                    // 说话触发的显示被拦住
    EXPECT_FALSE(f.ov.isVisible());
    f.ov.restoreFromUserHide();
    f.ov.showOnScreen();
    EXPECT_TRUE(f.ov.isVisible());
}

TEST(OverlayButtons, HideButtonCanBeDisabledWhenThereIsNoTray) {
    OverlayFixture f;
    f.ov.setHideButtonEnabled(false);
    f.hover();
    SignalCount hidden(&f.ov, &LiveOverlay::hiddenByUser);
    click(&f.ov, f.ov.hideButtonRect().center());
    EXPECT_EQ(hidden.count(), 0);
    EXPECT_TRUE(f.ov.isVisible());
}

TEST(OverlayButtons, LayoutIsHorizontalOnThePillAndVerticalOnATallCaptionCard) {
    OverlayFixture f;
    EXPECT_FALSE(f.ov.buttonsAreVertical());
    EXPECT_NEAR(f.ov.hideButtonRect().center().y(), f.ov.dragButtonRect().center().y(), 0.5);
    EXPECT_GT(f.ov.dragButtonRect().left(), f.ov.hideButtonRect().right());

    f.caption.setSpeaking(true);
    f.caption.setPartial(f.caption.epoch(), "This is a fairly long caption sentence that has to wrap onto a second line of the card for sure yes.");
    pump(900);
    EXPECT_TRUE(f.ov.buttonsAreVertical());
    EXPECT_NEAR(f.ov.hideButtonRect().center().x(), f.ov.dragButtonRect().center().x(), 0.5);
    EXPECT_GT(f.ov.dragButtonRect().top(), f.ov.hideButtonRect().bottom());
}

TEST(OverlayButtons, OnlyCardAndButtonsReceiveMouse) {
    OverlayFixture f;
    f.hover();
    const QRegion m = f.ov.mask();
    EXPECT_TRUE(m.contains(f.ov.cardRect().center().toPoint()));
    EXPECT_TRUE(m.contains(f.ov.hideButtonRect().center().toPoint()));
    EXPECT_TRUE(m.contains(f.ov.dragButtonRect().center().toPoint()));
    EXPECT_FALSE(m.contains(QPoint(3, f.ov.height() / 2)));   // 左侧留白不挡下面的窗口
}

TEST(OverlayButtons, DragMovesTheWindowSavesAndDoubleClickResets) {
    OverlayFixture f;
    f.hover();
    const QPoint before = f.ov.pos();
    const QPointF grab = f.ov.dragButtonRect().center();
    const QPointF g0 = f.ov.mapToGlobal(grab.toPoint());
    mouse(&f.ov, QEvent::MouseButtonPress, grab, Qt::LeftButton, {}, g0);
    mouse(&f.ov, QEvent::MouseMove, grab, Qt::LeftButton, {}, g0 + QPointF(-120, -60));
    mouse(&f.ov, QEvent::MouseButtonRelease, grab, Qt::LeftButton, {}, g0 + QPointF(-120, -60));
    EXPECT_EQ(f.ov.pos(), before + QPoint(-120, -60));
    ASSERT_TRUE(FloatingWindow::loadPosition("LiveOverlay").has_value());

    // 窗口随字幕变大后卡片贴边的那条线不动
    f.caption.setSpeaking(true);
    f.caption.setPartial(f.caption.epoch(), "A longer caption so the card grows a lot more than before to wrap to several lines of text here.");
    pump(900);
    const auto saved = FloatingWindow::loadPosition("LiveOverlay");
    EXPECT_NEAR(f.ov.x() + f.ov.width() / 2, saved->x(), 1);          // 中线不动
    EXPECT_NEAR(f.ov.y() + f.ov.height() - 30, saved->y(), 2);        // 贴边那条线不动

    f.hover();
    const QPointF g1 = f.ov.mapToGlobal(f.ov.dragButtonRect().center().toPoint());
    mouse(&f.ov, QEvent::MouseButtonDblClick, f.ov.dragButtonRect().center(), Qt::LeftButton, {}, g1);
    EXPECT_FALSE(FloatingWindow::loadPosition("LiveOverlay").has_value());
}


// ---------- 三个页面在横条面板与设置窗口之间来回借用 ----------
TEST(SharedPages, BorrowedBackAndForthKeepsInstancesAndOrder) {
    TempConfig cfg;
    ExpandPanel panel;
    LiveTranscriptWidget* live = new LiveTranscriptWidget;
    QWidget* dict = new QWidget;
    QWidget* file = new QWidget;
    QPointer<QWidget> pl(live), pd(dict), pf(file);
    panel.setPages(live, dict, file);
    live->appendUtterance("kept across moves");

    QTabWidget settings;   // 代替设置窗口的标签页
    for (int round = 0; round < 3; ++round) {
        panel.releaseSharedPages();
        settings.addTab(live, "live");
        settings.addTab(dict, "dict");
        settings.addTab(file, "file");
        ASSERT_EQ(settings.count(), 3);
        ASSERT_TRUE(pl && pd && pf);                  // 借走期间页面没被销毁

        settings.removeTab(settings.indexOf(live));
        settings.removeTab(settings.indexOf(dict));
        settings.removeTab(settings.indexOf(file));
        panel.takeBackSharedPages(live, dict, file);
        ASSERT_TRUE(pl && pd && pf);
        auto* tabs = panel.findChild<QTabWidget*>();
        ASSERT_NE(tabs, nullptr);
        ASSERT_EQ(tabs->count(), 3);
        EXPECT_EQ(tabs->widget(0), live);             // 顺序：实时转录、词典、文件转录
        EXPECT_EQ(tabs->widget(1), dict);
        EXPECT_EQ(tabs->widget(2), file);
    }
    auto* list = live->findChild<QListView*>("live_transcript_list");
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->model()->rowCount(), 1);          // 稿子状态留在实例里
}
