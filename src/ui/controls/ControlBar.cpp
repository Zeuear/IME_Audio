#include "ControlBar.h"
#include "ControlBarState.h"
#include "FloatingWindow.h"
#include "ToolIcon.h"
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QToolButton>

namespace {
constexpr int kButtonSize = 36;
constexpr int kIconSize = 22;
constexpr int kPadding = 8;
constexpr int kRadius = 12;
const QString kPositionGroup = QStringLiteral("ControlBar");

QToolButton* makeButton(QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setFixedSize(kButtonSize, kButtonSize);
    b->setIconSize(QSize(kIconSize, kIconSize));
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);   // 点击不拿键盘焦点
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
    layout->addWidget(m_startBtn);
    layout->addWidget(m_stopBtn);

    connect(m_startBtn, &QToolButton::clicked, this, &ControlBar::startRequested);
    connect(m_stopBtn, &QToolButton::clicked, this, &ControlBar::stopRequested);

    retranslateUi();
    setWorkflowState(WorkflowState::Idle);
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
}

void ControlBar::retranslateUi()
{
    m_startBtn->setToolTip(tr("Start"));
    m_stopBtn->setToolTip(tr("Stop"));
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
