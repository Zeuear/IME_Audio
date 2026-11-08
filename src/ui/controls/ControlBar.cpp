#include "ControlBar.h"
#include "ControlBarState.h"
#include "FloatingWindow.h"
#include "ToolIcon.h"
#include <QAbstractButton>
#include <QEvent>
#include <QHBoxLayout>
#include <QAction>
#include <QContextMenuEvent>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>

namespace {
constexpr int kButtonSize = 28;
constexpr int kIconSize = 18;
constexpr int kPadH = 12;
constexpr int kPadV = 5;
constexpr int kRadius = 12;
constexpr int kSpacing = 4;
constexpr int kGroupGap = 8;
const QString kPositionGroup = QStringLiteral("ControlBar");

QString buttonStyle(const QColor& accent)
{
    // 选中态用强调色半透明底；应用样式表自带的选中底色会盖住图标，所以逐个覆盖
    const QString on = QStringLiteral("rgba(%1, %2, %3, ").arg(accent.red()).arg(accent.green()).arg(accent.blue());
    return QStringLiteral(
        "QToolButton { background: transparent; border: none; border-radius: 8px; }"
        "QToolButton:hover:enabled { background: rgba(128, 128, 128, 45); }"
        "QToolButton:pressed:enabled { background: rgba(128, 128, 128, 80); }"
        "QToolButton:checked { background: ") + on + QStringLiteral("45); }"
        "QToolButton:checked:hover:enabled { background: ") + on + QStringLiteral("70); }");
}

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
    f->setFixedSize(1, 16);
    f->setStyleSheet(QStringLiteral("background: rgba(128, 128, 128, 80);"));
    return f;
}

QIcon tintedWith(const QString& resource, const QColor& color)
{
    const QImage img = ToolIcon::render(resource, color, kIconSize * 2);
    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(2.0);
    return QIcon(pm);
}

QIcon tinted(const QString& resource, const QPalette& palette, ToolIcon::State state)
{
    return tintedWith(resource, ToolIcon::stateColor(palette, state));
}
} // namespace

ControlBar::ControlBar(QWidget* parent)
    : QWidget(parent)
{
    FloatingWindow::applyToolWindowFlags(this);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(kPadH, kPadV, kPadH, kPadV);
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

    m_transcriptBtn->setCheckable(true);
    connect(m_transcriptBtn, &QToolButton::toggled, this, [this](bool open) {
        refreshIcons();
        emit transcriptToggled(open);
    });

    // 点击只请求，选中状态由宿主经 setPanelPage 回写校正
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

void ControlBar::setAlwaysOnTop(bool on)
{
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
    const QString style = buttonStyle(pal.color(QPalette::Active, QPalette::Highlight));
    for (QToolButton* b : { m_startBtn, m_stopBtn, m_enhanceBtn, m_translateBtn, m_injectBtn,
                            m_transcriptBtn, m_dictionaryBtn, m_fileBtn, m_settingsBtn }) {
        if (b->styleSheet() != style) b->setStyleSheet(style);
    }

    m_startBtn->setIcon(tinted(QStringLiteral(":/tool_record"), pal,
                               m_startBtn->isEnabled() ? ToolIcon::State::Record : ToolIcon::State::Disabled));
    m_stopBtn->setIcon(tinted(QStringLiteral(":/tool_stop"), pal,
                              m_stopBtn->isEnabled() ? ToolIcon::State::Record : ToolIcon::State::Disabled));

    auto toggleIcon = [&pal](const QString& resource, const QToolButton* b) {
        if (!b->isEnabled()) return tinted(resource, pal, ToolIcon::State::Disabled);
        return tinted(resource, pal, b->isChecked() ? ToolIcon::State::On : ToolIcon::State::Off);
    };
    m_enhanceBtn->setIcon(toggleIcon(QStringLiteral(":/tool_ai_enhance"), m_enhanceBtn));
    m_translateBtn->setIcon(toggleIcon(QStringLiteral(":/tool_translate"), m_translateBtn));
    m_injectBtn->setIcon(toggleIcon(QStringLiteral(":/tool_inject"), m_injectBtn));

    auto panelIcon = [&pal](const QString& resource, const QToolButton* b) {
        return tinted(resource, pal, b->isChecked() ? ToolIcon::State::On : ToolIcon::State::Normal);
    };
    m_transcriptBtn->setIcon(panelIcon(QStringLiteral(":/tool_transcript"), m_transcriptBtn));
    m_dictionaryBtn->setIcon(panelIcon(QStringLiteral(":/tool_dictionary"), m_dictionaryBtn));
    m_fileBtn->setIcon(panelIcon(QStringLiteral(":/tool_file"), m_fileBtn));
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
