#include "ControlBar.h"
#include "ControlBarState.h"
#include "FloatingWindow.h"
#include "ToolIcon.h"
#include <QAbstractButton>
#include <QEvent>
#include <QHBoxLayout>
#include <QAction>
#include <QContextMenuEvent>
#include <QIcon>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QToolButton>

namespace {
constexpr int kButtonSize = 36;
constexpr int kIconSize = 22;
constexpr int kPadding = 8;
constexpr int kRadius = 12;
constexpr int kGroupGap = 8;
const QString kPositionGroup = QStringLiteral("ControlBar");

QToolButton* makeButton(QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setFixedSize(kButtonSize, kButtonSize);
    b->setIconSize(QSize(kIconSize, kIconSize));
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);   // 点击不拿键盘焦点
    // 开关打开只靠图标变色表示：应用样式表会给选中的工具按钮铺一块高亮底色，把图标盖住，这里单独去掉
    b->setStyleSheet(QStringLiteral(
        "QToolButton { background: transparent; border: none; border-radius: 8px; }"
        "QToolButton:hover:enabled { background: rgba(128, 128, 128, 45); }"
        "QToolButton:pressed:enabled { background: rgba(128, 128, 128, 80); }"
        "QToolButton:checked { background: transparent; }"
        "QToolButton:checked:hover:enabled { background: rgba(128, 128, 128, 45); }"));
    return b;
}

QIcon tinted(const QString& resource, const QPalette& palette, ToolIcon::State state)
{
    const QImage img = ToolIcon::render(resource, ToolIcon::stateColor(palette, state), kIconSize * 2);
    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(2.0);
    return QIcon(pm);
}
} // namespace

ControlBar::ControlBar(QWidget* parent)
    : QWidget(parent)
{
    FloatingWindow::applyToolWindowFlags(this);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    layout->setSpacing(4);
    m_startBtn = makeButton(this);
    m_stopBtn = makeButton(this);
    m_enhanceBtn = makeButton(this);
    m_translateBtn = makeButton(this);
    m_injectBtn = makeButton(this);
    m_transcriptBtn = makeButton(this);
    m_dictionaryBtn = makeButton(this);
    m_fileBtn = makeButton(this);
    m_settingsBtn = makeButton(this);
    layout->addWidget(m_startBtn);
    layout->addWidget(m_stopBtn);
    layout->addSpacing(kGroupGap);   // 动作按钮和开关之间留一点距离，一眼分得出
    layout->addWidget(m_enhanceBtn);
    layout->addWidget(m_translateBtn);
    layout->addWidget(m_injectBtn);
    layout->addSpacing(kGroupGap);
    layout->addWidget(m_transcriptBtn);
    layout->addWidget(m_dictionaryBtn);
    layout->addWidget(m_fileBtn);
    layout->addWidget(m_settingsBtn);

    m_menu = new QMenu(this);
    m_settingsAction = m_menu->addAction(QString(), this, &ControlBar::settingsRequested);
    m_menu->addSeparator();
    m_quitAction = m_menu->addAction(QString(), this, &ControlBar::quitRequested);

    for (QToolButton* b : { m_enhanceBtn, m_translateBtn, m_injectBtn }) {
        b->setCheckable(true);
        connect(b, &QToolButton::toggled, this, &ControlBar::refreshIcons);
        // 开关的可用状态由宿主（ToggleLink）改，变化时图标要跟着变灰或恢复
        b->installEventFilter(this);
    }

    connect(m_startBtn, &QToolButton::clicked, this, &ControlBar::startRequested);
    connect(m_stopBtn, &QToolButton::clicked, this, &ControlBar::stopRequested);
    connect(m_settingsBtn, &QToolButton::clicked, this, &ControlBar::settingsRequested);

    m_transcriptBtn->setCheckable(true);
    connect(m_transcriptBtn, &QToolButton::toggled, this, [this](bool open) {
        refreshIcons();
        emit transcriptToggled(open);
    });

    // 面板图标：点击只"请求"，状态由宿主通过 setPanelPage 回写，所以自己点出来的选中状态马上会被校正
    m_dictionaryBtn->setCheckable(true);
    m_fileBtn->setCheckable(true);
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
    // 宿主已经决定了面板状态：只把图标对齐过去，不再发 panelRequested
    const QSignalBlocker blockDict(m_dictionaryBtn);
    const QSignalBlocker blockFile(m_fileBtn);
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

void ControlBar::setTranscriptOpen(bool open)
{
    // 窗口是别处关的：只改图标，不能再发 transcriptToggled，否则宿主会再收到一次"关闭"
    const QSignalBlocker blocker(m_transcriptBtn);
    m_transcriptBtn->setChecked(open);
    refreshIcons();
}

QAbstractButton* ControlBar::settingsButton() const
{
    return m_settingsBtn;
}

void ControlBar::contextMenuEvent(QContextMenuEvent* event)
{
    m_menu->exec(event->globalPos());
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
    refreshIcons();
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
    m_startBtn->setIcon(tinted(QStringLiteral(":/tool_play"), pal,
                               m_startBtn->isEnabled() ? ToolIcon::State::Normal : ToolIcon::State::Disabled));
    m_stopBtn->setIcon(tinted(QStringLiteral(":/tool_stop"), pal,
                              m_stopBtn->isEnabled() ? ToolIcon::State::Normal : ToolIcon::State::Disabled));

    // 开关：关闭用弱化色，开启用强调色，不可用变灰
    auto toggleState = [](const QToolButton* b) {
        if (!b->isEnabled()) return ToolIcon::State::Disabled;
        return b->isChecked() ? ToolIcon::State::On : ToolIcon::State::Off;
    };
    m_enhanceBtn->setIcon(tinted(QStringLiteral(":/tool_ai_enhance"), pal, toggleState(m_enhanceBtn)));
    m_translateBtn->setIcon(tinted(QStringLiteral(":/tool_translate"), pal, toggleState(m_translateBtn)));
    m_injectBtn->setIcon(tinted(QStringLiteral(":/tool_inject"), pal, toggleState(m_injectBtn)));
    // 转录内容：窗口开着高亮，关着用普通色；录音中同样可点
    m_transcriptBtn->setIcon(tinted(QStringLiteral(":/tool_transcript"), pal,
                                    m_transcriptBtn->isChecked() ? ToolIcon::State::On : ToolIcon::State::Normal));
    m_dictionaryBtn->setIcon(tinted(QStringLiteral(":/tool_dictionary"), pal,
                                    m_dictionaryBtn->isChecked() ? ToolIcon::State::On : ToolIcon::State::Normal));
    m_fileBtn->setIcon(tinted(QStringLiteral(":/tool_file"), pal,
                              m_fileBtn->isChecked() ? ToolIcon::State::On : ToolIcon::State::Normal));
    // 设置入口始终可点：录音中也要能打开
    m_settingsBtn->setIcon(tinted(QStringLiteral(":/tool_settings"), pal, ToolIcon::State::Normal));
}

void ControlBar::retranslateUi()
{
    m_startBtn->setToolTip(tr("Start"));
    m_stopBtn->setToolTip(tr("Stop"));
    m_transcriptBtn->setToolTip(tr("Live Transcript"));
    m_settingsBtn->setToolTip(tr("Settings"));
    m_settingsAction->setText(tr("Settings"));
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
    // 底色和边框取自调色板，light / gray / dark 主题都可读
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor border = palette().color(QPalette::WindowText);
    border.setAlpha(50);
    p.setPen(QPen(border, 1));
    p.setBrush(palette().color(QPalette::Window));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
}

void ControlBar::changeEvent(QEvent* event)
{
    switch (event->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
        refreshIcons();   // 主题切换走调色板，图标要按新颜色重画
        update();
        break;
    default:
        break;
    }
    QWidget::changeEvent(event);
}

void ControlBar::mousePressEvent(QMouseEvent* event)
{
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
