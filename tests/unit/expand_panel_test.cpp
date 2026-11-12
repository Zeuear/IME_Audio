#include <gtest/gtest.h>
#include <QAbstractButton>
#include <QCoreApplication>
#include <QImage>
#include <QLabel>
#include <QList>
#include <QMouseEvent>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QTabWidget>
#include <QWidget>

#include "ui/controls/ControlBar.h"
#include "ui/controls/ExpandPanel.h"
#include "ui/controls/FloatingWindow.h"
#include "ui/controls/PanelState.h"
#include "ui/infobar/appleshadoweffect.h"

// QApplication 由 support/widget_app_environment.cpp 里的全局环境提供

using PanelState::Page;

// ---- 点图标后面板该是什么状态 ----

TEST(PanelState, ClosedPanelOpensTheClickedPage) {
    EXPECT_EQ(PanelState::afterClick(Page::None, Page::Transcript), Page::Transcript);
    EXPECT_EQ(PanelState::afterClick(Page::None, Page::Dictionary), Page::Dictionary);
    EXPECT_EQ(PanelState::afterClick(Page::None, Page::File), Page::File);
}

TEST(PanelState, ClickingTheCurrentPagesIconCollapsesThePanel) {
    EXPECT_EQ(PanelState::afterClick(Page::Dictionary, Page::Dictionary), Page::None);
    EXPECT_EQ(PanelState::afterClick(Page::File, Page::File), Page::None);
}

TEST(PanelState, ClickingTheOtherIconSwitchesPages) {
    EXPECT_EQ(PanelState::afterClick(Page::Dictionary, Page::File), Page::File);
    EXPECT_EQ(PanelState::afterClick(Page::File, Page::Dictionary), Page::Dictionary);
}

// ---- 面板摆在横条下面 ----

namespace {
const QRect kMain(0, 0, 1920, 1040);
const QRect kRight(1920, 0, 1280, 1000);
const QSize kPanel(780, 520);
constexpr int kGap = 6;
} // namespace

TEST(FloatingWindowPlaceBelow, SitsRightUnderTheBarAlignedToItsLeftEdge) {
    const QRect bar(500, 12, 300, 48);
    const QPoint p = FloatingWindow::placeBelow(bar, kPanel, { kMain }, kMain);
    EXPECT_EQ(p, QPoint(500, 12 + 48 + kGap));
}

TEST(FloatingWindowPlaceBelow, PulledLeftWhenItWouldRunOffTheRightEdge) {
    const QRect bar(1700, 12, 300, 48);
    const QPoint p = FloatingWindow::placeBelow(bar, kPanel, { kMain }, kMain);
    EXPECT_EQ(p.x(), 1920 - 780);
    EXPECT_EQ(p.y(), 12 + 48 + kGap);
}

TEST(FloatingWindowPlaceBelow, FlipsAboveTheBarWhenThereIsNoRoomBelow) {
    const QRect bar(500, 900, 300, 48);
    const QPoint p = FloatingWindow::placeBelow(bar, kPanel, { kMain }, kMain);
    EXPECT_EQ(p, QPoint(500, 900 - kGap - 520));
}

TEST(FloatingWindowPlaceBelow, StaysOnTheScreenHoldingTheBar) {
    const QRect bar(3000, 12, 300, 48);   // 在右侧副屏的右端
    const QPoint p = FloatingWindow::placeBelow(bar, kPanel, { kMain, kRight }, kMain);
    EXPECT_EQ(p.x(), 3200 - 780);
    EXPECT_EQ(p.y(), 12 + 48 + kGap);
}

TEST(FloatingWindowPlaceBelow, BarOutsideEveryScreenFallsBackToPrimary) {
    const QRect bar(9000, 12, 300, 48);
    const QPoint p = FloatingWindow::placeBelow(bar, kPanel, { kMain }, kMain);
    EXPECT_TRUE(kMain.contains(p));
    EXPECT_TRUE(kMain.contains(p + QPoint(kPanel.width() - 1, kPanel.height() - 1)));
}

// ---- 面板本身：页面由外部传入，所以这里用普通 QWidget 代替词典和文件转录 ----

namespace {
struct PanelFixture {
    PanelFixture() {
        transcript = new QWidget;
        dictionary = new QWidget;
        file = new QWidget;
        panel.setPages(transcript, dictionary, file);   // 页面归面板所有
        QObject::connect(&panel, &ExpandPanel::pageChanged, &panel, [this](Page p) { changes << p; });
    }
    QTabWidget* tabs() { return panel.findChild<QTabWidget*>(); }

    ExpandPanel panel;
    QWidget* transcript = nullptr;
    QWidget* dictionary = nullptr;
    QWidget* file = nullptr;
    QList<Page> changes;
};
} // namespace

TEST(ExpandPanel, StartsCollapsedAndSettingPagesAnnouncesNothing) {
    // 回归：QTabWidget 加第一页时会发 currentChanged(0)，不能把它当成"打开了词典页"，
    // 否则面板还没开 page() 就是词典，第一次点词典图标反而被算成"收起"
    ExpandPanel panel;
    QList<Page> changes;
    QObject::connect(&panel, &ExpandPanel::pageChanged, &panel, [&](Page p) { changes << p; });
    panel.setPages(new QWidget, new QWidget, new QWidget);
    EXPECT_EQ(panel.page(), Page::None);
    EXPECT_FALSE(panel.isVisible());
    EXPECT_TRUE(changes.isEmpty());
}

TEST(ExpandPanel, FirstClickOnADictionaryIconOpensItFromAFreshPanel) {
    // 宿主的做法：点图标 → afterClick(当前页, 点的页) → showPage
    ExpandPanel panel;
    panel.setPages(new QWidget, new QWidget, new QWidget);
    panel.showPage(PanelState::afterClick(panel.page(), Page::Dictionary));
    EXPECT_TRUE(panel.isVisible());
    EXPECT_EQ(panel.page(), Page::Dictionary);
}

TEST(ExpandPanel, TabChangesWhileCollapsedAreNotAnnounced) {
    PanelFixture f;
    f.tabs()->setCurrentIndex(2);   // 面板没开时，程序或别的原因改了当前 tab
    EXPECT_EQ(f.panel.page(), Page::None);
    EXPECT_TRUE(f.changes.isEmpty());
}

TEST(ExpandPanel, ShowPageOpensThatPage) {
    PanelFixture f;
    f.panel.showPage(Page::Dictionary);
    EXPECT_TRUE(f.panel.isVisible());
    EXPECT_EQ(f.panel.page(), Page::Dictionary);
    EXPECT_EQ(f.tabs()->currentWidget(), f.dictionary);

    f.panel.showPage(Page::File);
    EXPECT_EQ(f.panel.page(), Page::File);
    EXPECT_EQ(f.tabs()->currentWidget(), f.file);
}

TEST(ExpandPanel, ShowingNonePageCollapsesIt) {
    PanelFixture f;
    f.panel.showPage(Page::File);
    f.panel.showPage(Page::None);
    EXPECT_FALSE(f.panel.isVisible());
    EXPECT_EQ(f.panel.page(), Page::None);
}

TEST(ExpandPanel, EachChangeIsAnnouncedExactlyOnce) {
    // 横条据此点亮图标；程序切页和用户点 tab 都要报，但不能重复报
    PanelFixture f;
    f.panel.showPage(Page::Dictionary);
    f.tabs()->setCurrentIndex(2);          // 用户点了面板里的 tab
    f.panel.showPage(Page::None);
    EXPECT_EQ(f.changes, (QList<Page>{ Page::Dictionary, Page::File, Page::None }));
}

TEST(ExpandPanel, HidingItAnyOtherWayAlsoReportsNone) {
    PanelFixture f;
    f.panel.showPage(Page::Dictionary);
    f.panel.hide();
    EXPECT_EQ(f.panel.page(), Page::None);
    EXPECT_EQ(f.changes, (QList<Page>{ Page::Dictionary, Page::None }));
}

TEST(ExpandPanel, OpeningItDoesNotStealFocusFromTheDictationTarget) {
    // 点图标开面板不能把前台窗口切走；用户自己点进输入框时才拿焦点
    PanelFixture f;
    EXPECT_TRUE(f.panel.testAttribute(Qt::WA_ShowWithoutActivating));
}

TEST(ExpandPanel, UsesAppleShadowAndKeepsRoundedBody) {
    ExpandPanel panel;
    auto* shadow = qobject_cast<AppleShadowEffect*>(panel.graphicsEffect());
    ASSERT_NE(shadow, nullptr);
    EXPECT_EQ(shadow->cornerRadius(), 8);
}

TEST(ExpandPanel, ResizeCursorIsAvailableAlongRightAndBottomEdges) {
    ExpandPanel panel;
    panel.showPage(Page::Dictionary);

    const QPoint rightEdge(panel.width() - 8, panel.height() / 2);
    QMouseEvent rightMove(QEvent::MouseMove, rightEdge, panel.mapToGlobal(rightEdge),
                          Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&panel, &rightMove);
    EXPECT_EQ(panel.cursor().shape(), Qt::SizeHorCursor);

    const QPoint bottomEdge(panel.width() / 2, panel.height() - 8);
    QMouseEvent bottomMove(QEvent::MouseMove, bottomEdge, panel.mapToGlobal(bottomEdge),
                           Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&panel, &bottomMove);
    EXPECT_EQ(panel.cursor().shape(), Qt::SizeVerCursor);
}

TEST(ExpandPanel, PageContentIsVisibleAndCornersAreRoundedOff) {
    auto makePage = [](const QColor& c) {
        auto* w = new QWidget;
        w->setAutoFillBackground(true);
        QPalette pal = w->palette();
        pal.setColor(QPalette::Window, c);
        w->setPalette(pal);
        return w;
    };
    ExpandPanel panel;
    panel.setPages(makePage(Qt::red), makePage(Qt::red), makePage(Qt::red));
    panel.showPage(Page::Dictionary);
    QCoreApplication::processEvents();

    const QImage img = panel.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    const QColor center = img.pixelColor(img.width() / 2, img.height() / 2);
    EXPECT_GT(center.red(), 200) << "页面内容不可见";
    EXPECT_GT(center.alpha(), 250);
    // 圆角外的页面直角不能露出来：body 左上角第一个像素在圆角之外
    EXPECT_LT(img.pixelColor(16, 16 + 1).red() * img.pixelColor(16, 16 + 1).alpha(), 200 * 255);
    // 阴影边距不能完全透明，否则分层窗口不收鼠标
    EXPECT_GE(img.pixelColor(2, img.height() / 2).alpha(), 1);
}

// ---- 横条上的词典 / 文件转录图标 ----

TEST(ControlBarPanelIcons, ClickingAnIconAsksForThatPage) {
    ControlBar bar;
    QList<Page> asked;
    QObject::connect(&bar, &ControlBar::panelRequested, &bar, [&](Page p) { asked << p; });
    ASSERT_NE(bar.dictionaryButton(), nullptr);
    ASSERT_NE(bar.fileButton(), nullptr);

    bar.dictionaryButton()->click();
    bar.fileButton()->click();
    EXPECT_EQ(asked, (QList<Page>{ Page::Dictionary, Page::File }));
}

TEST(ControlBarPanelIcons, HostSetsWhichIconIsLitWithoutAskingAgain) {
    ControlBar bar;
    int asked = 0;
    QObject::connect(&bar, &ControlBar::panelRequested, &bar, [&](Page) { ++asked; });

    bar.setPanelPage(Page::Dictionary);
    EXPECT_TRUE(bar.dictionaryButton()->isChecked());
    EXPECT_FALSE(bar.fileButton()->isChecked());

    bar.setPanelPage(Page::File);
    EXPECT_FALSE(bar.dictionaryButton()->isChecked());
    EXPECT_TRUE(bar.fileButton()->isChecked());

    bar.setPanelPage(Page::None);
    EXPECT_FALSE(bar.dictionaryButton()->isChecked());
    EXPECT_FALSE(bar.fileButton()->isChecked());
    EXPECT_EQ(asked, 0);
}

TEST(ControlBarPanelIcons, StayUsableWhileDictating) {
    ControlBar bar;
    for (auto st : { WorkflowState::Idle, WorkflowState::Recording, WorkflowState::Stopping, WorkflowState::Error }) {
        bar.setWorkflowState(st);
        EXPECT_TRUE(bar.dictionaryButton()->isEnabled()) << static_cast<int>(st);
        EXPECT_TRUE(bar.fileButton()->isEnabled()) << static_cast<int>(st);
    }
}

// ---- 置顶开关：关掉后窗口不再盖在别的软件上面，但已经开着的不能消失、不能跳位置 ----

namespace {
template <typename Window>
void expectOnTopToggleKeepsWindowInPlace(Window& w)
{
    w.show();
    const QPoint where(140, 90);
    w.move(where);
    EXPECT_TRUE(w.windowFlags() & Qt::WindowStaysOnTopHint);   // 默认置顶

    w.setAlwaysOnTop(false);
    EXPECT_FALSE(w.windowFlags() & Qt::WindowStaysOnTopHint);
    EXPECT_TRUE(w.isVisible());
    EXPECT_EQ(w.pos(), where);

    w.setAlwaysOnTop(true);
    EXPECT_TRUE(w.windowFlags() & Qt::WindowStaysOnTopHint);
    EXPECT_TRUE(w.isVisible());
    EXPECT_EQ(w.pos(), where);
}
} // namespace

TEST(ControlBarOnTop, ToggleKeepsTheBarVisibleAndInPlace) {
    ControlBar bar;
    expectOnTopToggleKeepsWindowInPlace(bar);
}

TEST(ControlBarOnTop, StaysNonActivatingAfterTheFlagChanges) {
    // 改窗口标志会重建原生窗口：不抢焦点的性质不能丢，否则点横条会切走前台窗口
    ControlBar bar;
    bar.show();
    bar.setAlwaysOnTop(false);
    EXPECT_TRUE(bar.windowFlags() & Qt::WindowDoesNotAcceptFocus);
    EXPECT_TRUE(bar.testAttribute(Qt::WA_ShowWithoutActivating));
}

TEST(ExpandPanelOnTop, ToggleKeepsThePanelVisibleAndInPlace) {
    ExpandPanel panel;
    panel.setPages(new QWidget, new QWidget, new QWidget);
    expectOnTopToggleKeepsWindowInPlace(panel);
}

TEST(ExpandPanelOnTop, ToggleDoesNotCollapseOrAnnounceAnything) {
    ExpandPanel panel;
    panel.setPages(new QWidget, new QWidget, new QWidget);
    QList<Page> changes;
    QObject::connect(&panel, &ExpandPanel::pageChanged, &panel, [&](Page p) { changes << p; });
    panel.showPage(Page::File);
    changes.clear();

    panel.setAlwaysOnTop(false);   // 改标志会让 Qt 先隐藏再显示，不能被当成"面板被收起"
    EXPECT_EQ(panel.page(), Page::File);
    EXPECT_TRUE(panel.isVisible());
    EXPECT_TRUE(changes.isEmpty());
}

// ---- 面板大小：可拖动调整，停下后存进配置，下次打开沿用 ----

#include <QTemporaryDir>
#include <QEventLoop>
#include <QTimer>
#include "config/ConfigManager.h"

namespace {
class PanelTempConfig {
public:
    PanelTempConfig() : m_old(ConfigManager::configFilePath()) {
        ConfigManager::setConfigFilePath(m_dir.filePath("test.ini"));
    }
    ~PanelTempConfig() { ConfigManager::setConfigFilePath(m_old); }
private:
    QTemporaryDir m_dir;
    QString m_old;
};
} // namespace

TEST(ExpandPanelSize, UsesTheSavedSizeWhenThereIsOne) {
    PanelTempConfig cfg;
    FloatingWindow::saveSize("ExpandPanel", QSize(1000, 700));
    ExpandPanel panel;
    EXPECT_EQ(panel.size(), QSize(1000, 700));
}

TEST(ExpandPanelSize, ASavedSizeTooSmallToUseIsRaisedToTheMinimum) {
    PanelTempConfig cfg;
    FloatingWindow::saveSize("ExpandPanel", QSize(50, 40));
    ExpandPanel panel;
    EXPECT_EQ(panel.size(), panel.minimumSize());
}

TEST(ExpandPanelSize, ResizingIsSavedForTheNextRun) {
    PanelTempConfig cfg;
    {
        ExpandPanel panel;
        panel.setPages(new QWidget, new QWidget, new QWidget);
        panel.showPage(Page::Transcript);
        panel.resize(900, 640);
        QEventLoop loop;   // 保存有防抖，等它触发
        QTimer::singleShot(600, &loop, &QEventLoop::quit);
        loop.exec();
    }
    EXPECT_EQ(FloatingWindow::loadSize("ExpandPanel"), std::optional<QSize>(QSize(900, 640)));
}
