#include "ExpandPanel.h"
#include "FloatingWindow.h"
#include "ui/infobar/appleshadoweffect.h"
#include <QEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace {
constexpr int kShadowMargin = 16;
constexpr int kCornerRadius = 8;
constexpr QSize kPanelSize(780 + 2 * kShadowMargin, 520 + 2 * kShadowMargin);
constexpr QSize kMinPanelSize(480 + 2 * kShadowMargin, 300 + 2 * kShadowMargin);
constexpr int kCornerGrip = 16;   // 右下角判定范围放大，好抓
const QString kSettingsGroup = QStringLiteral("ExpandPanel");
constexpr int kTranscriptIndex = 0;
constexpr int kDictionaryIndex = 1;
constexpr int kFileIndex = 2;
}

// 盖在最上层：页面控件是直角的，会从圆角处探出来，这里把圆角外的部分擦掉，再描边
class ExpandPanel::CornerOverlay : public QWidget
{
public:
    explicit CornerOverlay(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF body = QRectF(rect()).adjusted(kShadowMargin, kShadowMargin,
                                                    -kShadowMargin, -kShadowMargin);
        QPainterPath outside;
        outside.addRect(body);
        QPainterPath rounded;
        rounded.addRoundedRect(body, kCornerRadius, kCornerRadius);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillPath(outside.subtracted(rounded), Qt::black);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setPen(QPen(palette().color(QPalette::Mid), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(body.adjusted(0.5, 0.5, -0.5, -0.5), kCornerRadius, kCornerRadius);
    }
};

int ExpandPanel::indexOf(PanelState::Page page)
{
    switch (page) {
    case PanelState::Page::Transcript: return kTranscriptIndex;
    case PanelState::Page::Dictionary: return kDictionaryIndex;
    default: return kFileIndex;
    }
}

PanelState::Page ExpandPanel::pageAt(int index)
{
    switch (index) {
    case kTranscriptIndex: return PanelState::Page::Transcript;
    case kDictionaryIndex: return PanelState::Page::Dictionary;
    default: return PanelState::Page::File;
    }
}

ExpandPanel::ExpandPanel(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    // 打开时不激活：前台窗口不被切走，听写注入目标不变；点进输入框时才拿焦点
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_QuitOnClose, false);
    setAutoFillBackground(false);

    auto* shadow = new AppleShadowEffect(this);
    shadow->setBlurRadius(kShadowMargin - 3);
    shadow->setInnerDistance(kShadowMargin);
    shadow->setOffset(QPointF(0, 3));
    shadow->setColor(QColor(0, 0, 0, 90));
    shadow->setCornerRadius(kCornerRadius);
    setGraphicsEffect(shadow);

    m_tabs = new QTabWidget(this);
    m_tabs->setStyleSheet(QStringLiteral(
        "QTabWidget { background: transparent; }"
        "QTabWidget::pane { border-radius: %1px; }"
        "QTabBar::tab:first { border-top-left-radius: %1px; }"
        "QTabBar::tab:last { border-top-right-radius: %1px; }").arg(kCornerRadius));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kShadowMargin, kShadowMargin, kShadowMargin, kShadowMargin);
    layout->addWidget(m_tabs);
    m_overlay = new CornerOverlay(this);
    connect(m_tabs, &QTabWidget::currentChanged, this, &ExpandPanel::onTabChanged);

    setMouseTracking(true);
    setMinimumSize(kMinPanelSize);
    resize(FloatingWindow::loadSize(kSettingsGroup).value_or(kPanelSize));
    m_savedSize = size();

    m_sizeSaveTimer = new QTimer(this);
    m_sizeSaveTimer->setSingleShot(true);
    m_sizeSaveTimer->setInterval(400);
    connect(m_sizeSaveTimer, &QTimer::timeout, this, [this]() {
        m_savedSize = size();
        FloatingWindow::saveSize(kSettingsGroup, m_savedSize);
        if (isVisible()) reposition();   // 变大后可能超出屏幕或压到横条
    });
}

void ExpandPanel::setPages(QWidget* transcript, QWidget* dictionary, QWidget* file)
{
    // 加第一页时 QTabWidget 会发 currentChanged(0)，那不是用户打开了某一页，不能当成状态变化
    m_syncingTab = true;
    m_tabs->addTab(transcript, QString());
    m_tabs->addTab(dictionary, QString());
    m_tabs->addTab(file, QString());
    m_syncingTab = false;
    retranslateUi();
}

void ExpandPanel::setAlwaysOnTop(bool on)
{
    m_changingFlags = true;
    FloatingWindow::setAlwaysOnTop(this, on);
    m_changingFlags = false;
}

void ExpandPanel::attachTo(QWidget* anchor)
{
    if (m_anchor) m_anchor->removeEventFilter(this);
    m_anchor = anchor;
    if (m_anchor) m_anchor->installEventFilter(this);
    if (parentWidget() != anchor) setParent(anchor, windowFlags());
}

void ExpandPanel::showPage(PanelState::Page page)
{
    if (page == PanelState::Page::None) {
        hide();   // hideEvent 负责把状态复位并报告，避免这里再报一次
        return;
    }

    const bool wasOpen = isVisible();
    const bool changed = page != m_page;
    m_page = page;

    m_syncingTab = true;
    m_tabs->setCurrentIndex(indexOf(page));
    m_syncingTab = false;

    if (!wasOpen) {
        reposition();
        show();
        // 不激活的窗口 show 不会排到最前；没开置顶时会出现在别的窗口后面，看起来像没打开
        if (m_anchor) m_anchor->raise();
        raise();
    }
    if (changed || !wasOpen) emit pageChanged(m_page);
}

void ExpandPanel::onTabChanged(int index)
{
    // 程序切页时 showPage 自己会报；面板收着时 tab 变了也不算打开了哪一页
    if (m_syncingTab || !isVisible()) return;
    const PanelState::Page page = pageAt(index);
    if (page == m_page) return;
    m_page = page;
    emit pageChanged(m_page);
}

void ExpandPanel::reposition()
{
    if (!m_anchor) return;
    // 锚点窗口四周若留了透明阴影边距，要按可见主体对齐，否则面板离横条太远
    const int inset = m_anchor->property("visualInset").toInt();
    const QPoint position = FloatingWindow::placeBelow(
        m_anchor->frameGeometry().adjusted(inset, inset, -inset, -inset),
        size() - QSize(2 * kShadowMargin, 2 * kShadowMargin),   // 按可见主体摆放，阴影不算
        FloatingWindow::availableScreens(), FloatingWindow::primaryScreenArea());
    move(position - QPoint(kShadowMargin, kShadowMargin));
}

void ExpandPanel::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    // 最小化等自发的隐藏、改置顶标志时的临时隐藏，都不算收起
    if (event->spontaneous() || m_changingFlags || m_page == PanelState::Page::None) return;
    m_page = PanelState::Page::None;
    emit pageChanged(m_page);
}

void ExpandPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_overlay->setGeometry(rect());
    m_overlay->raise();
    if (size() != m_savedSize) m_sizeSaveTimer->start();
}

Qt::Edges ExpandPanel::resizeEdgesAt(const QPoint& pos) const
{
    const bool right = pos.x() >= width() - kShadowMargin;
    const bool bottom = pos.y() >= height() - kShadowMargin;
    Qt::Edges edges;
    if (right || (bottom && pos.x() >= width() - kCornerGrip)) edges |= Qt::RightEdge;
    if (bottom || (right && pos.y() >= height() - kCornerGrip)) edges |= Qt::BottomEdge;
    return edges;
}

void ExpandPanel::mouseMoveEvent(QMouseEvent* event)
{
    const Qt::Edges edges = resizeEdgesAt(event->position().toPoint());
    if (edges == (Qt::RightEdge | Qt::BottomEdge)) setCursor(Qt::SizeFDiagCursor);
    else if (edges == Qt::RightEdge) setCursor(Qt::SizeHorCursor);
    else if (edges == Qt::BottomEdge) setCursor(Qt::SizeVerCursor);
    else unsetCursor();
    QWidget::mouseMoveEvent(event);
}

void ExpandPanel::mousePressEvent(QMouseEvent* event)
{
    const Qt::Edges edges = resizeEdgesAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && edges && windowHandle()) {
        windowHandle()->startSystemResize(edges);
        return;
    }
    QWidget::mousePressEvent(event);
}

void ExpandPanel::leaveEvent(QEvent* event)
{
    unsetCursor();
    QWidget::leaveEvent(event);
}

void ExpandPanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        showPage(PanelState::Page::None);
        return;
    }
    QWidget::keyPressEvent(event);
}

bool ExpandPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_anchor && isVisible()) {
        switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
            reposition();
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ExpandPanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF body = QRectF(rect()).adjusted(kShadowMargin, kShadowMargin,
                                                -kShadowMargin, -kShadowMargin);
    // 阴影边距必须保证非零透明度：分层窗口里完全透明的像素不接收鼠标，
    // 边缘光标和拖拽缩放会时有时无
    p.fillRect(rect(), QColor(0, 0, 0, 1));
    p.setPen(Qt::NoPen);
    p.setBrush(palette().color(QPalette::Window));
    p.drawRoundedRect(body, kCornerRadius, kCornerRadius);
}

void ExpandPanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QWidget::changeEvent(event);
}

void ExpandPanel::retranslateUi()
{
    if (m_tabs->count() < 3) return;
    m_tabs->setTabText(kTranscriptIndex, tr("Live Transcript"));
    m_tabs->setTabText(kDictionaryIndex, tr("Dictionary"));
    m_tabs->setTabText(kFileIndex, tr("File Transcription"));
}
