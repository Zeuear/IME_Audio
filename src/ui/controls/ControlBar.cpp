#include "ControlBar.h"
#include "ControlBarState.h"
#include "FloatingWindow.h"
#include "ToolIcon.h"
#include "services/ForegroundWindow.h"
#include "utils/Logger.h"
#include "ui/infobar/appleshadoweffect.h"
#include <QAbstractButton>
#include <QElapsedTimer>
#include <QEvent>
#include <QHBoxLayout>
#include <QAction>
#include <QContextMenuEvent>
#include <QFrame>
#include <QIcon>
#include <QLinearGradient>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>

namespace {
constexpr int kButtonSize = 38;
constexpr int kIconSize = 36;   // 带底板的图标整块尺寸
constexpr int kPadH = 14;
constexpr int kPadV = 5;
constexpr int kShadow = 10;   // 窗口四周留给阴影的透明边距
constexpr int kSpacing = 4;
constexpr int kGroupGap = 8;
const QString kPositionGroup = QStringLiteral("ControlBar");

// 状态和悬停都画在图标自带的底板上，按钮本身不再有底色；
// 应用样式表自带的选中 / 悬停底色会盖住图标，所以逐个覆盖成透明
const char* const kButtonStyle =
    "QToolButton, QToolButton:hover, QToolButton:pressed, QToolButton:checked {"
    " background: transparent; border: none; }"
    "QToolButton:pressed { padding-top: 1px; }";

// 开启 / 进行中的底板色：录音用红，所有开关和面板入口统一用蓝，避免一排花花绿绿
const QColor kRed(0xff, 0x45, 0x3a);
const QColor kBlue(0x0a, 0x84, 0xff);

QToolButton* makeButton(QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setFixedSize(kButtonSize, kButtonSize);
    b->setIconSize(QSize(kIconSize, kIconSize));
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

QFrame* makeDivider(QWidget* parent)
{
    auto* f = new QFrame(parent);
    // 短而淡，只做分组提示，不抢图标的视觉
    f->setFixedSize(1, 18);
    f->setStyleSheet(QStringLiteral("background: rgba(128, 128, 128, 40);"));
    return f;
}

QPixmap tilePixmap(const QString& resource, const QPalette& palette, const QColor& accent,
                   const QColor& glyph, bool hover)
{
    QPixmap pm = QPixmap::fromImage(ToolIcon::renderTile(resource, kIconSize * 2, palette, accent, glyph, hover));
    pm.setDevicePixelRatio(2.0);
    return pm;
}

// accent 有效画彩色底板；无效画中性底板，图案取 state 对应的颜色。
// 悬停时 Qt 取 Active 模式的图，所以两张都备好
QIcon tile(const QString& resource, const QPalette& palette, const QColor& accent, ToolIcon::State state)
{
    const QColor glyph = ToolIcon::stateColor(palette, state);
    QIcon icon;
    icon.addPixmap(tilePixmap(resource, palette, accent, glyph, false), QIcon::Normal);
    icon.addPixmap(tilePixmap(resource, palette, accent, glyph, true), QIcon::Active);
    return icon;
}
} // namespace

ControlBar::ControlBar(QWidget* parent)
    : QWidget(parent)
{
    FloatingWindow::applyToolWindowFlags(this);
    setAttribute(Qt::WA_AlwaysShowToolTips);   // 非激活的工具窗口默认不显示悬停提示
    setProperty("visualInset", kShadow);       // 告诉 ExpandPanel：四周这圈是透明阴影，对齐时扣掉

    m_shadow = new AppleShadowEffect(this);
    m_shadow->setBlurRadius(kShadow - 3);
    m_shadow->setInnerDistance(kShadow);
    m_shadow->setOffset(QPointF(0, 3));
    m_shadow->setColor(QColor(0, 0, 0, 90));
    setGraphicsEffect(m_shadow);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(kPadH + kShadow, kPadV + kShadow, kPadH + kShadow, kPadV + kShadow);
    layout->setSpacing(kSpacing);
    m_startBtn = makeButton(this);
    m_stopBtn = makeButton(this);
    m_enhanceBtn = makeButton(this);
    m_translateBtn = makeButton(this);
    m_injectBtn = makeButton(this);
    m_transcriptBtn = makeButton(this);
    m_dictionaryBtn = makeButton(this);
    m_fileBtn = makeButton(this);
    m_settingsBtn = makeButton(this);

    m_timerLabel = new QLabel(this);
    m_timerLabel->setMinimumWidth(42);
    m_timerLabel->setAlignment(Qt::AlignCenter);
    QFont mono = m_timerLabel->font();
    mono.setStyleHint(QFont::Monospace);
    mono.setFamily(QStringLiteral("Consolas"));
    mono.setPointSizeF(mono.pointSizeF() + 1);
    m_timerLabel->setFont(mono);

    layout->addWidget(m_startBtn);
    layout->addWidget(m_stopBtn);
    layout->addWidget(m_timerLabel);
    layout->addSpacing(kGroupGap);
    layout->addWidget(makeDivider(this));
    layout->addSpacing(kGroupGap);
    layout->addWidget(m_enhanceBtn);
    layout->addWidget(m_translateBtn);
    layout->addWidget(m_injectBtn);
    layout->addSpacing(kGroupGap);
    layout->addWidget(makeDivider(this));
    layout->addSpacing(kGroupGap);
    layout->addWidget(m_transcriptBtn);
    layout->addWidget(m_dictionaryBtn);
    layout->addWidget(m_fileBtn);
    layout->addSpacing(kGroupGap);
    layout->addWidget(makeDivider(this));
    layout->addSpacing(kGroupGap);
    layout->addWidget(m_settingsBtn);

    m_ticker = new QTimer(this);
    m_ticker->setInterval(1000);
    connect(m_ticker, &QTimer::timeout, this, &ControlBar::tick);

    m_menu = new QMenu(this);
    m_settingsAction = m_menu->addAction(QString(), this, &ControlBar::settingsRequested);
    m_topAction = m_menu->addAction(QString());
    m_topAction->setCheckable(true);
    m_topAction->setChecked(true);
    // 改置顶标志会销毁重建原生窗口；此刻菜单的 exec 还没返回，在里面重建会让菜单挂在已销毁的窗口上，
    // 表现为菜单错位、关不掉或置顶丢失。推迟到菜单收起、事件循环空闲后再发
    connect(m_topAction, &QAction::triggered, this, [this](bool on) {
        LOG_DEBUG(QString("ControlBar topAction triggered checked=%1 flagOn=%2").arg(on).arg(bool(windowFlags() & Qt::WindowStaysOnTopHint)));
        QTimer::singleShot(0, this, [this, on]() {
            setAlwaysOnTop(on);   // 之前只发信号，横条自己的窗口标志没人改，取消勾选后仍然置顶
            emit alwaysOnTopToggled(on);
        });
    });
    m_docsAction = m_menu->addAction(QString());
    m_docsAction->setCheckable(true);
    connect(m_docsAction, &QAction::triggered, this, &ControlBar::docsOutputToggled);
    m_docsPairAction = m_menu->addAction(QString(), this, &ControlBar::docsPairingCodeRequested);
    m_docsInstallAction = m_menu->addAction(QString(), this, &ControlBar::docsExtensionRequested);
    m_menu->addSeparator();
    m_quitAction = m_menu->addAction(QString(), this, &ControlBar::quitRequested);

    for (QToolButton* b : { m_enhanceBtn, m_translateBtn, m_injectBtn }) {
        b->setCheckable(true);
        connect(b, &QToolButton::toggled, this, &ControlBar::refreshIcons);
        // 宿主会改可用状态，图标要跟着变灰或恢复
        b->installEventFilter(this);
    }

    connect(m_startBtn, &QToolButton::clicked, this, &ControlBar::startRequested);
    connect(m_stopBtn, &QToolButton::clicked, this, &ControlBar::stopRequested);
    connect(m_settingsBtn, &QToolButton::clicked, this, &ControlBar::settingsRequested);

    // 点击只请求，选中状态由宿主经 setPanelPage 回写校正
    m_transcriptBtn->setCheckable(true);
    m_dictionaryBtn->setCheckable(true);
    m_fileBtn->setCheckable(true);
    connect(m_transcriptBtn, &QToolButton::toggled, this, [this]() { emit panelRequested(PanelState::Page::Transcript); });
    connect(m_transcriptBtn, &QToolButton::toggled, this, &ControlBar::refreshIcons);
    connect(m_dictionaryBtn, &QToolButton::toggled, this, [this]() { emit panelRequested(PanelState::Page::Dictionary); });
    connect(m_fileBtn, &QToolButton::toggled, this, [this]() { emit panelRequested(PanelState::Page::File); });
    connect(m_dictionaryBtn, &QToolButton::toggled, this, &ControlBar::refreshIcons);
    connect(m_fileBtn, &QToolButton::toggled, this, &ControlBar::refreshIcons);

    retranslateUi();
    setWorkflowState(WorkflowState::Idle);
}

QAbstractButton* ControlBar::dictionaryButton() const
{
    return m_dictionaryBtn;
}

QAbstractButton* ControlBar::fileButton() const
{
    return m_fileBtn;
}

void ControlBar::setPanelPage(PanelState::Page page)
{
    const QSignalBlocker blockTranscript(m_transcriptBtn);
    const QSignalBlocker blockDict(m_dictionaryBtn);
    const QSignalBlocker blockFile(m_fileBtn);
    m_transcriptBtn->setChecked(page == PanelState::Page::Transcript);
    m_dictionaryBtn->setChecked(page == PanelState::Page::Dictionary);
    m_fileBtn->setChecked(page == PanelState::Page::File);
    refreshIcons();
}

void ControlBar::setFileBusy(bool busy)
{
    m_fileBusy = busy;
    refreshToggleTips();
}

QAbstractButton* ControlBar::transcriptButton() const
{
    return m_transcriptBtn;
}

QAbstractButton* ControlBar::settingsButton() const
{
    return m_settingsBtn;
}

void ControlBar::contextMenuEvent(QContextMenuEvent* event)
{
    LOG_DEBUG(QString("ControlBar ctxMenu reason=%1 menuVisible=%2 pos=%3,%4").arg(int(event->reason())).arg(m_menu->isVisible()).arg(event->globalPos().x()).arg(event->globalPos().y()));
    // 已经开着就不再叠一个
    if (m_menu->isVisible()) return;

    // 横条不可激活，本进程此时通常不是前台。弹出菜单要靠激活来持有鼠标捕获，
    // 没有前台权限就会被系统立刻收回，表现为菜单一闪而过。先让本进程拿到前台，再弹菜单
    const ForegroundWindow::Handle previous = ForegroundWindow::current();
    ForegroundWindow::bringToFront(winId());
    QElapsedTimer et; et.start();
    const QAction* chosen = m_menu->exec(event->globalPos());
    LOG_DEBUG(QString("ControlBar ctxMenu exec returned after %1 ms chosen=%2").arg(et.elapsed()).arg(chosen != nullptr));

    // 菜单收起后把横条顶回最前（raise 不激活）；没选任何项时把前台还给原来的窗口，听写目标不丢
    raise();
    if (!chosen && previous) ForegroundWindow::restore(previous);
}

QToolButton* ControlBar::button(Toggle which) const
{
    switch (which) {
    case Toggle::Enhance:   return m_enhanceBtn;
    case Toggle::Translate: return m_translateBtn;
    case Toggle::Inject:    return m_injectBtn;
    }
    return nullptr;
}

QAbstractButton* ControlBar::toggleButton(Toggle which) const
{
    return button(which);
}

void ControlBar::setToggleHint(Toggle which, const QString& hint)
{
    m_hints[static_cast<int>(which)] = hint;
    refreshToggleTips();
}

bool ControlBar::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::EnabledChange && qobject_cast<QToolButton*>(watched)) {
        refreshIcons();
    }
    return QWidget::eventFilter(watched, event);
}

void ControlBar::setWorkflowState(WorkflowState state)
{
    const ControlBarState::Buttons b = ControlBarState::forWorkflow(state);
    m_startBtn->setEnabled(b.startEnabled);
    m_stopBtn->setEnabled(b.stopEnabled);

    // 连续听写会在录音和转录间来回切，只在会话首尾动计时
    if (ControlBarState::isSessionActive(state)) {
        if (!m_ticker->isActive()) {
            m_elapsedSec = 0;
            m_ticker->start();
        }
    } else {
        m_ticker->stop();
        m_elapsedSec = 0;
    }
    refreshTimerLabel();
    refreshIcons();
}

bool ControlBar::timerRunning() const
{
    return m_ticker->isActive();
}

QString ControlBar::elapsedText() const
{
    return ControlBarState::formatElapsed(m_elapsedSec);
}

void ControlBar::tick()
{
    ++m_elapsedSec;
    refreshTimerLabel();
}

void ControlBar::refreshTimerLabel()
{
    const QString text = elapsedText();
    const bool widthChanged = text.size() != m_timerLabel->text().size();
    m_timerLabel->setText(text);
    QPalette pal = m_timerLabel->palette();
    const auto state = m_ticker->isActive() ? ToolIcon::State::Record : ToolIcon::State::Off;
    pal.setColor(QPalette::WindowText, ToolIcon::stateColor(palette(), state));
    m_timerLabel->setPalette(pal);
    if (widthChanged && isVisible()) adjustSize();
}

void ControlBar::setDocsBinding(const QString& title)
{
    m_docsTitle = title;
    m_docsAction->setText(title.isEmpty() ? tr("Output to Google Docs")
                                          : tr("Output to Google Docs (%1)").arg(title));
}

void ControlBar::setAlwaysOnTop(bool on)
{
    LOG_DEBUG(QString("ControlBar setAlwaysOnTop(%1) flagBefore=%2 visible=%3").arg(on).arg(bool(windowFlags() & Qt::WindowStaysOnTopHint)).arg(isVisible()));
    const QSignalBlocker blocker(m_topAction);
    m_topAction->setChecked(on);
    FloatingWindow::setAlwaysOnTop(this, on);
}

void ControlBar::showAtSavedPosition()
{
    adjustSize();
    FloatingWindow::restore(this, kPositionGroup);
    show();
}

void ControlBar::refreshIcons()
{
    const QPalette& pal = palette();
    const QString style = QLatin1String(kButtonStyle);
    for (QToolButton* b : { m_startBtn, m_stopBtn, m_enhanceBtn, m_translateBtn, m_injectBtn,
                            m_transcriptBtn, m_dictionaryBtn, m_fileBtn, m_settingsBtn }) {
        if (b->styleSheet() != style) b->setStyleSheet(style);
    }

    const QColor none;   // 无效色 = 中性底板
    using S = ToolIcon::State;

    // 录音红只给当前可点的那一个，避免两个红块并排
    m_startBtn->setIcon(tile(QStringLiteral(":/tool_record"), pal,
                             m_startBtn->isEnabled() ? kRed : none,
                             m_startBtn->isEnabled() ? S::Normal : S::Disabled));
    m_stopBtn->setIcon(tile(QStringLiteral(":/tool_stop"), pal,
                            m_stopBtn->isEnabled() ? kRed : none,
                            m_stopBtn->isEnabled() ? S::Normal : S::Disabled));

    // 开关：打开 = 彩色底板，关闭 = 中性底板配暗色图案
    auto toggleIcon = [&](const QString& resource, const QToolButton* b, const QColor& accent) {
        if (!b->isEnabled()) return tile(resource, pal, none, S::Disabled);
        return b->isChecked() ? tile(resource, pal, accent, S::Normal) : tile(resource, pal, none, S::Off);
    };
    m_enhanceBtn->setIcon(toggleIcon(QStringLiteral(":/tool_ai_enhance"), m_enhanceBtn, kBlue));
    m_translateBtn->setIcon(toggleIcon(QStringLiteral(":/tool_translate"), m_translateBtn, kBlue));
    m_injectBtn->setIcon(toggleIcon(QStringLiteral(":/tool_inject"), m_injectBtn, kBlue));

    // 面板入口：展开 = 彩色底板，收起 = 中性底板
    auto panelIcon = [&](const QString& resource, const QToolButton* b, const QColor& accent) {
        return tile(resource, pal, b->isChecked() ? accent : none, S::Normal);
    };
    m_transcriptBtn->setIcon(panelIcon(QStringLiteral(":/tool_transcript"), m_transcriptBtn, kBlue));
    m_dictionaryBtn->setIcon(panelIcon(QStringLiteral(":/tool_dictionary"), m_dictionaryBtn, kBlue));
    m_fileBtn->setIcon(panelIcon(QStringLiteral(":/tool_file"), m_fileBtn, kBlue));
    m_settingsBtn->setIcon(tile(QStringLiteral(":/tool_settings"), pal, none, S::Normal));
}

void ControlBar::retranslateUi()
{
    m_startBtn->setToolTip(tr("Start"));
    m_stopBtn->setToolTip(tr("Stop"));
    m_transcriptBtn->setToolTip(tr("Live Transcript"));
    m_settingsBtn->setToolTip(tr("Settings"));
    m_settingsAction->setText(tr("Settings"));
    m_topAction->setText(tr("Always on top"));
    setDocsBinding(m_docsTitle);
    m_docsPairAction->setText(tr("Copy Google Docs Pairing Code"));
    m_docsInstallAction->setText(tr("Install Google Docs Extension..."));
    m_quitAction->setText(tr("Quit"));
    refreshToggleTips();
}

void ControlBar::refreshToggleTips()
{
    const QString names[3] = { tr("AI Enhancement"), tr("AI Translation"), tr("Type into the foreground window") };
    QToolButton* buttons[3] = { m_enhanceBtn, m_translateBtn, m_injectBtn };
    for (int i = 0; i < 3; ++i) {
        buttons[i]->setToolTip(m_hints[i].isEmpty() ? names[i] : names[i] + QLatin1Char('\n') + m_hints[i]);
    }
    m_dictionaryBtn->setToolTip(tr("Dictionary"));
    m_fileBtn->setToolTip(m_fileBusy ? tr("File Transcription") + QLatin1Char('\n') + tr("Transcribing...")
                                     : tr("File Transcription"));
}

void ControlBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF body = QRectF(rect()).adjusted(kShadow, kShadow, -kShadow, -kShadow);
    const qreal radius = body.height() / 2;   // 胶囊形

    // 阴影由 m_shadow（AppleShadowEffect）画在本窗口之下，这里只画主体；
    // 主体必须不透明，否则阴影会从底下透出来
    QColor border = palette().color(QPalette::WindowText);
    border.setAlpha(40);
    const QColor fill = palette().color(QPalette::Window);
    p.setPen(QPen(border, 1));
    p.setBrush(fill);
    const QRectF edge = body.adjusted(0.5, 0.5, -0.5, -0.5);
    p.drawRoundedRect(edge, radius, radius);

    // 顶部一圈内高光，向下渐隐；浅色主题下白色高光看不出来，不影响
    QLinearGradient hi(edge.topLeft(), edge.bottomLeft());
    hi.setColorAt(0.0, QColor(255, 255, 255, 60));
    hi.setColorAt(0.5, QColor(255, 255, 255, 0));
    p.setPen(QPen(QBrush(hi), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(edge.adjusted(1, 1, -1, -1), radius - 1, radius - 1);
}

void ControlBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // 胶囊形：阴影轮廓的圆角跟着主体高度走
    m_shadow->setCornerRadius((height() - 2 * kShadow) / 2.0);
}

void ControlBar::changeEvent(QEvent* event)
{
    switch (event->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
        refreshIcons();
        refreshTimerLabel();
        update();
        break;
    default:
        break;
    }
    QWidget::changeEvent(event);
}

void ControlBar::mousePressEvent(QMouseEvent* event)
{
    // 没勾选置顶时横条会被盖住；点到露出来的部分就顶到前面（raise 不激活，不抢听写目标的焦点）
    if (!(windowFlags() & Qt::WindowStaysOnTopHint)) raise();
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QWidget::mousePressEvent(event);
}

void ControlBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) move(event->globalPosition().toPoint() - m_dragOffset);
    QWidget::mouseMoveEvent(event);
}

void ControlBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging) {
        m_dragging = false;
        FloatingWindow::savePosition(kPositionGroup, pos());
    }
    QWidget::mouseReleaseEvent(event);
}
