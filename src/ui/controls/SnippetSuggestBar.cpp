#include "SnippetSuggestBar.h"

#include <QEasingCurve>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QScreen>
#include <QToolTip>
#include <QVariantAnimation>

namespace {

constexpr int kAutoHideMs = 8000;
constexpr int kMaxLabelWidth = 160;
constexpr int kGapToAnchor = 8;
constexpr int kCollapsedSize = 36;      // 收起时的圆形箭头直径
constexpr int kCollapseDelayMs = 400;   // 鼠标移出后延迟收起，给从箭头移向按钮留缓冲
constexpr int kAnimMs = 260;
constexpr int kMarginX = 10;
constexpr int kMarginY = 8;
constexpr int kButtonH = 28;
constexpr int kButtonPadX = 12;
constexpr int kSpacing = 6;
constexpr qreal kClickableProgress = 0.85;   // 展开到这个进度之后按钮才响应点击

qreal lerp(qreal a, qreal b, qreal t) { return a + (b - a) * t; }

QRectF lerpRect(const QRectF& a, const QRectF& b, qreal t) {
    return QRectF(lerp(a.x(), b.x(), t), lerp(a.y(), b.y(), t), lerp(a.width(), b.width(), t),
                  lerp(a.height(), b.height(), t));
}

} // namespace

SnippetSuggestBar::SnippetSuggestBar(QWidget *parent) : QWidget(parent) {
    // 和 LiveOverlay 一样：不能成为焦点窗口，否则目标应用失去前台，粘贴就落空
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMouseTracking(true);
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
#endif

    m_anim = new QVariantAnimation(this);
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_progress = v.toReal();
        update();
    });
    connect(m_anim, &QVariantAnimation::finished, this, [this]() {
        if (m_expanded) return;
        applyPlacement();   // 收起播完：补上动画期间推迟的定位，并恢复遮罩
        if (!m_pending.isEmpty()) {
            const QVector<Suggestion> pending = m_pending;
            m_pending.clear();
            setSuggestions(pending);
        }
    });

    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(kAutoHideMs);
    connect(&m_hideTimer, &QTimer::timeout, this, &SnippetSuggestBar::closeAndRemember);

    m_collapseTimer.setSingleShot(true);
    m_collapseTimer.setInterval(kCollapseDelayMs);
    connect(&m_collapseTimer, &QTimer::timeout, this, [this]() { setExpanded(false); });
}

void SnippetSuggestBar::setSuggestions(const QVector<Suggestion>& suggestions) {
    QStringList ids;
    for (const Suggestion& s : suggestions) ids << s.id;
    if (ids.isEmpty()) {
        dismiss();
        return;
    }
    if (ids == m_currentIds && isVisible()) {
        m_pending.clear();
        return;
    }
    // 鼠标在栏上时按钮不能换位置，否则正要点的那个可能变成另一条，等收起后再显示新的
    if (isVisible() && underMouse()) {
        m_pending = suggestions;
        return;
    }
    m_currentIds = ids;

    // 按钮不是控件，而是自绘的：这样才能被底板的轮廓裁剪，随展开逐步露出
    m_items.clear();
    const QFontMetrics fm(font());
    int x = kMarginX;
    for (const Suggestion& s : suggestions) {
        Item item;
        item.id = s.id;
        item.label = fm.elidedText(s.label, Qt::ElideRight, kMaxLabelWidth);
        item.tooltip = s.tooltip;
        const int w = fm.horizontalAdvance(item.label) + 2 * kButtonPadX;
        item.finalRect = QRectF(x, kMarginY, w, kButtonH);
        x += w + kSpacing;
        m_items.push_back(item);
    }
    m_full = QSize(x - kSpacing + kMarginX, kButtonH + 2 * kMarginY).expandedTo(QSize(kCollapsedSize, kCollapsedSize));
    m_hover = -1;

    // 新一批提示从收起的小箭头开始，不播放动画
    m_anim->stop();
    m_expanded = false;
    m_progress = 0;
    applyPlacement();
    update();
    show();
    m_hideTimer.start();
}

void SnippetSuggestBar::placeNear(const QRect& anchor, bool below) {
    m_anchor = anchor;
    m_anchorBelow = below;
    // 字幕变长时锚点会一直变，展开/动画期间跟着动就是抖动，等收起后再补上
    if (!m_expanded && m_anim->state() != QAbstractAnimation::Running) applyPlacement();
}

// 窗口固定为展开后的完整尺寸，动画只在 paintEvent 里画：逐帧改原生窗口的几何，
// 窗口和内容的刷新不同步，无论怎么取整都会抖。这里只在收起且静止时调用，所以同时设回遮罩。
void SnippetSuggestBar::applyPlacement() {
    if (m_full.isEmpty()) return;
    const bool flipped = m_below != m_anchorBelow;
    m_below = m_anchorBelow;
    QRect target(pos(), m_full);
    if (!m_anchor.isNull()) {
        QScreen* screen = QGuiApplication::screenAt(m_anchor.center());
        if (!screen) screen = QGuiApplication::primaryScreen();
        const QRect area = screen->availableGeometry();
        const int w = m_full.width();
        const int h = m_full.height();
        const int x = m_anchor.center().x() - w / 2;
        const int y = m_below ? m_anchor.bottom() + kGapToAnchor : m_anchor.top() - h - kGapToAnchor;
        target.moveTo(qBound(area.left(), x, area.right() - w), qBound(area.top(), y, area.bottom() - h));
    }
    if (target != geometry()) setGeometry(target);
    // 收起时只有箭头区域接收鼠标，否则透明部分会挡住下面的窗口并误触发展开。
    // 多留 1px：系统缩放非整数时遮罩取整不能切到圆的边缘
    // 字幕每次变化都会走到这里，遮罩没变就不碰原生窗口
    const QRegion collapsedMask(collapsedRect().adjusted(-1, -1, 1, 1));
    if (mask() != collapsedMask) setMask(collapsedMask);
    if (flipped) update();
}

// 圆形箭头在窗口内的位置：贴锚点的那条边、水平居中
QRect SnippetSuggestBar::collapsedRect() const {
    return QRect((m_full.width() - kCollapsedSize) / 2, m_below ? 0 : m_full.height() - kCollapsedSize,
                 kCollapsedSize, kCollapsedSize);
}

QRectF SnippetSuggestBar::buttonRect(int i) const {
    return m_items[i].finalRect;
}

int SnippetSuggestBar::buttonAt(const QPoint& pos) const {
    if (m_progress < kClickableProgress) return -1;
    for (int i = 0; i < m_items.size(); ++i)
        if (buttonRect(i).contains(pos)) return i;
    return -1;
}

void SnippetSuggestBar::setExpanded(bool expanded) {
    if (m_expanded == expanded) return;
    m_expanded = expanded;
    if (expanded) clearMask();
    m_anim->stop();
    m_anim->setEasingCurve(expanded ? QEasingCurve::OutCubic : QEasingCurve::InCubic);
    m_anim->setStartValue(m_progress);
    m_anim->setEndValue(expanded ? 1.0 : 0.0);
    m_anim->setDuration(expanded ? kAnimMs : qRound(kAnimMs * m_progress) + 40);
    m_anim->start();
}

void SnippetSuggestBar::closeAndRemember() {
    const QStringList ids = m_currentIds;
    dismiss();
    if (!ids.isEmpty()) emit closed(ids);
}

void SnippetSuggestBar::dismiss() {
    m_hideTimer.stop();
    m_collapseTimer.stop();
    m_anim->stop();
    m_currentIds.clear();
    m_pending.clear();
    m_expanded = false;
    m_progress = 0;
    hide();
}

void SnippetSuggestBar::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    emit visibilityChanged(true);
}

void SnippetSuggestBar::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    emit visibilityChanged(false);
}

void SnippetSuggestBar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette& pal = palette();
    const qreal t = qBound(0.0, m_progress, 1.0);

    // 底板：圆形 → 按钮栏的圆角矩形。用小数坐标靠抗锯齿平滑过渡，不按整像素跳
    const QRectF collapsed = QRectF(collapsedRect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QRectF body = lerpRect(collapsed, QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), t);
    const qreal radius = qMin(lerp(kCollapsedSize / 2.0, 10, t), qMin(body.width(), body.height()) / 2);
    QPainterPath bodyPath;
    bodyPath.addRoundedRect(body, radius, radius);
    p.fillPath(bodyPath, pal.color(QPalette::Window));

    // 箭头：展开一开始就迅速淡出，让位给按钮
    const qreal arrowAlpha = 1.0 - qBound(0.0, t * 3.0, 1.0);
    if (arrowAlpha > 0) {
        p.save();
        p.setOpacity(arrowAlpha);
        const QPointF c = collapsed.center();
        const qreal dir = m_below ? 1.0 : -1.0;   // 在锚点下方时箭头朝下
        p.setPen(QPen(pal.color(QPalette::Text), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(QPolygonF({ QPointF(c.x() - 6, c.y() + 3 * dir), QPointF(c.x(), c.y() - 3 * dir),
                                   QPointF(c.x() + 6, c.y() + 3 * dir) }));

        if (m_items.size() > 1) {
            // 角标不能超出箭头区域，否则收起时被遮罩切掉、展开瞬间又露出来
            const QRectF badge(c.x() + kCollapsedSize / 2.0 - 16, c.y() - kCollapsedSize / 2.0, 16, 16);
            p.setPen(Qt::NoPen);
            p.setBrush(pal.color(QPalette::Highlight));
            p.drawEllipse(badge);
            QFont f = font();
            f.setPointSizeF(qMax(7.0, f.pointSizeF() - 2));
            f.setBold(true);
            p.setFont(f);
            p.setPen(pal.color(QPalette::HighlightedText));
            p.drawText(badge, Qt::AlignCenter, QString::number(m_items.size()));
        }
        p.restore();
    }

    // 按钮：大小位置固定，用底板的圆角轮廓裁剪，随轮廓扩大逐步露出；整体轻微淡入避免生硬
    const qreal alpha = qBound(0.0, (t - 0.15) / 0.5, 1.0);
    if (alpha > 0) {
        p.save();
        p.setClipPath(bodyPath);
        p.setOpacity(alpha);
        for (int i = 0; i < m_items.size(); ++i) {
            const QRectF r = buttonRect(i);
            const bool hovered = i == m_hover;
            p.setBrush(hovered ? pal.color(QPalette::Highlight) : pal.color(QPalette::Button));
            p.setPen(pal.color(QPalette::Mid));
            p.drawRoundedRect(r, 7, 7);

            p.setPen(pal.color(hovered ? QPalette::HighlightedText : QPalette::ButtonText));
            p.drawText(r, Qt::AlignCenter, m_items[i].label);
        }
        p.restore();
    }

    // 边框最后画：按钮被揭开时不会盖住底板的轮廓线
    p.setBrush(Qt::NoBrush);
    p.setPen(pal.color(QPalette::Mid));
    p.drawPath(bodyPath);
}

void SnippetSuggestBar::mouseMoveEvent(QMouseEvent *event) {
    const int hover = buttonAt(event->position().toPoint());
    if (hover != m_hover) {
        m_hover = hover;
        setCursor(hover >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void SnippetSuggestBar::mouseReleaseEvent(QMouseEvent *event) {
    const int i = event->button() == Qt::LeftButton ? buttonAt(event->position().toPoint()) : -1;
    if (i >= 0) {
        const QString id = m_items[i].id;
        emit snippetClicked(id);
        closeAndRemember();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

bool SnippetSuggestBar::event(QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        const int i = buttonAt(static_cast<QHelpEvent*>(event)->pos());
        if (i >= 0 && !m_items[i].tooltip.isEmpty()) QToolTip::showText(static_cast<QHelpEvent*>(event)->globalPos(), m_items[i].tooltip, this);
        else QToolTip::hideText();
        return true;
    }
    return QWidget::event(event);
}

void SnippetSuggestBar::enterEvent(QEnterEvent *event) {
    m_hideTimer.stop();   // 鼠标在上面时不自动消失
    m_collapseTimer.stop();
    setExpanded(true);
    QWidget::enterEvent(event);
}

void SnippetSuggestBar::leaveEvent(QEvent *event) {
    if (m_hover != -1) {
        m_hover = -1;
        update();
    }
    if (isVisible()) {
        m_hideTimer.start();
        m_collapseTimer.start();
    }
    QWidget::leaveEvent(event);
}
