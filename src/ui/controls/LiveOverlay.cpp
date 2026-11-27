#include "LiveOverlay.h"
#include "FloatingWindow.h"
#include "ui/models/LiveCaptionModel.h"
#include "ToolIcon.h"
#include <QEnterEvent>
#include <QRegion>
#include <QFontMetricsF>
#include <QCursor>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTextBlock>
#include <QTextLayout>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kBallSize = 120;
constexpr qreal kPillHeight = 60.0;
constexpr qreal kEdgeInset = 30.0;   // 卡片贴屏幕的那条边与窗口边缘的距离，保持原 120x120 窗口里的位置
constexpr qreal kPadH = 18.0, kPadTop = 12.0, kPadBottom = 12.0, kTextBarsGap = 8.0;
constexpr qreal kBarsRowHeight = 14.0;
constexpr qreal kMinCardWidth = 220.0;
constexpr int kBarCount = 16;
constexpr qreal kBarSpacing = 1.0;
constexpr qreal kBarMargin = 12.0;
constexpr qreal kMinBarHeight = 5.0;
constexpr qreal kDotCycleMs = 1120.0;
constexpr qreal kIconHoldSec = 1.5;
constexpr int kSideGutter = 64;           // 卡片左右各留的空白，悬停按钮画在右侧；左右对称，卡片才保持居中
constexpr qreal kBtnSize = 24.0, kBtnGap = 4.0, kBtnOffset = 8.0;
constexpr qreal kBtnVerticalNeed = 2 * kBtnSize + kBtnGap + 16.0;
const QString kPosGroup = QStringLiteral("LiveOverlay");

qreal approach(qreal cur, qreal target, qreal tauSec, qreal dtSec)
{
    return target + (cur - target) * std::exp(-dtSec / tauSec);
}

qreal fade(qreal cur, bool on, qreal sec, qreal dtSec)
{
    const qreal step = dtSec / sec;
    return on ? std::min(1.0, cur + step) : std::max(0.0, cur - step);
}

qreal lerp(qreal a, qreal b, qreal t) { return a + (b - a) * t; }

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(float(lerp(a.redF(), b.redF(), t)), float(lerp(a.greenF(), b.greenF(), t)),
                            float(lerp(a.blueF(), b.blueF(), t)), float(lerp(a.alphaF(), b.alphaF(), t)));
}

QColor accentAt(qreal x01)
{
    return mix(QColor(0x3b, 0x82, 0xf6), QColor(0x8b, 0x5c, 0xf6), std::clamp(x01, 0.0, 1.0));
}

qreal dotOpacity(int index, qreal cycleMs)
{
    const qreal t = cycleMs - index * 160.0;
    if (t < 0) return 0.3;
    if (t < 400) {
        const qreal x = t / 400.0;
        return 0.3 + 0.7 * (x * (2.0 - x));
    }
    if (t < 800) {
        const qreal x = (t - 400.0) / 400.0;
        return 1.0 - 0.7 * (x * x);
    }
    return 0.3;
}

struct OverlayStyle {
    QColor bgTop, bgBottom, border;
    QColor finalText, pendingText;
    QColor barLoading, barPaused, barTranscribe;
};

const OverlayStyle &styleFor(LiveOverlay::Theme theme)
{
    static const OverlayStyle dark = {
        QColor(24, 26, 32, 244), QColor(17, 19, 24, 244), QColor(255, 255, 255, 40),
        QColor(0xff, 0xff, 0xff), QColor(0x7f, 0x87, 0x96),
        QColor(0x8e, 0xc6, 0xff), QColor(0x5b, 0x61, 0x6d), QColor(0xff, 0xc1, 0x07)};
    static const OverlayStyle gray = {
        QColor(214, 216, 221, 246), QColor(202, 204, 210, 246), QColor(0, 0, 0, 50),
        QColor(0x10, 0x12, 0x16), QColor(0x6b, 0x72, 0x80),
        QColor(0x3b, 0x82, 0xf6), QColor(0x8b, 0x90, 0x99), QColor(0xb8, 0x74, 0x00)};
    static const OverlayStyle light = {
        QColor(255, 255, 255, 248), QColor(245, 246, 249, 248), QColor(0, 0, 0, 36),
        QColor(0x15, 0x17, 0x1a), QColor(0x8d, 0x94, 0x9e),
        QColor(0x3b, 0x82, 0xf6), QColor(0xa0, 0xa5, 0xad), QColor(0xd9, 0x9a, 0x00)};
    switch (theme) {
    case LiveOverlay::Theme::Gray: return gray;
    case LiveOverlay::Theme::Light: return light;
    default: return dark;
    }
}
}

LiveOverlay::LiveOverlay(QWidget *parent)
    : QWidget(parent)
{
    // 只做状态显示，不能成为焦点窗口，否则听写结果会注入到它自己身上
    FloatingWindow::applyToolWindowFlags(this);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    resize(kBallSize + 2 * kSideGutter, kBallSize);

    m_barEnergy.fill(0.0, kBarCount);

    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(16);
    connect(m_animTimer, &QTimer::timeout, this, &LiveOverlay::tick);

    m_idleFadeTimer = new QTimer(this);
    m_idleFadeTimer->setSingleShot(true);
    m_idleFadeTimer->setInterval(4000);
    connect(m_idleFadeTimer, &QTimer::timeout, this, [this]() {
        if (m_hover || m_dragging) {
            m_idleFadeTimer->start();
            return;
        }
        if (m_state != Paused) {
            hideOverlay();
        }
    });
}

void LiveOverlay::setLevel(float normalizedLevel)
{
    m_level = std::clamp<qreal>(normalizedLevel, 0.0, 1.0);
}

void LiveOverlay::setSpectrumLevels(const QVector<float>& bands)
{
    m_levels = bands;
}

void LiveOverlay::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    update();   // 暂停 / 转录中的圆点颜色是瞬时切换，不会触发动画量变化
}
void LiveOverlay::setLoading()   { setState(Loading); }
void LiveOverlay::setListening() { setState(Listening); }
void LiveOverlay::setTranscribe(){ setState(Transcribe); }
void LiveOverlay::setPaused()    { setState(Paused); }

void LiveOverlay::setTheme(Theme theme)
{
    m_theme = theme;
    relayoutCaption();
    update();
}

QScreen *LiveOverlay::targetScreen() const
{
    return m_screen ? m_screen.data() : QGuiApplication::primaryScreen();
}

void LiveOverlay::positionOnScreen()
{
    if (m_anchor) {
        QScreen *screen = QGuiApplication::screenAt(*m_anchor);
        if (!screen) screen = targetScreen();
        if (!screen) return;
        const QRect geo = screen->availableGeometry();
        // 用户拖过的位置：锚点是窗口中线与卡片贴边那条线的交点，窗口大小变化时锚点不动，卡片就不会跳
        const int nx = std::clamp(m_anchor->x() - width() / 2, geo.left(), std::max(geo.left(), geo.right() - width() + 1));
        const int ay = m_atTop ? m_anchor->y() - int(kEdgeInset) : m_anchor->y() - height() + int(kEdgeInset);
        const int ny = std::clamp(ay, geo.top() - int(kEdgeInset), std::max(geo.top(), geo.bottom() - height() + int(kEdgeInset) + 1));
        move(nx, ny);
        return;
    }
    QScreen *screen = targetScreen();
    if (!screen) return;
    QRect geo = screen->availableGeometry();
    constexpr int margin = -5;
    const int y = m_atTop ? geo.top() + margin : geo.bottom() - height() - margin;
    move(geo.center().x() - width() / 2, y);
}

void LiveOverlay::setCaptionFontSize(int pt)
{
    m_fontPt = pt;
    relayoutCaption();
    update();
}

void LiveOverlay::setAtTop(bool atTop)
{
    m_atTop = atTop;
    if (isVisible()) positionOnScreen();
    update();
}

qreal LiveOverlay::cardTop(qreal cardH) const
{
    return m_atTop ? kEdgeInset : height() - kEdgeInset - cardH;
}

void LiveOverlay::showOnScreen()
{
    m_hideDeferred = false;
    if (m_userHidden) return;   // 用户点了隐藏：直到托盘里选「显示」之前都不再弹出
    if (!isVisible()) {
        m_screen = QGuiApplication::screenAt(QCursor::pos());
        relayoutCaption();
    }
    positionOnScreen();
    show();
#ifndef Q_OS_MACOS
    raise();
#endif
}

void LiveOverlay::hideOverlay()
{
    if (m_hideHeld) {
        m_hideDeferred = true;
        return;
    }
    m_hideDeferred = false;
    hide();
}

void LiveOverlay::setHideHeld(bool held)
{
    m_hideHeld = held;
    if (held || !m_hideDeferred) return;
    // 排到下一轮再隐藏：新一轮录音会先收起指令条、紧接着 showOnScreen，不能在中间闪一下
    QTimer::singleShot(0, this, [this]() {
        if (!m_hideHeld && m_hideDeferred) hideOverlay();
    });
}

void LiveOverlay::hideTimerStart() { m_idleFadeTimer->start(); }
void LiveOverlay::hideTimerStop() { m_idleFadeTimer->stop(); }

void LiveOverlay::onVoiceStarted()
{
    m_idleFadeTimer->stop();
    setState(Listening);
    showOnScreen();
}

void LiveOverlay::onVoiceStopped()
{
    m_idleFadeTimer->start();
}

void LiveOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_clock.restart();
    m_animTimer->start();
}

void LiveOverlay::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_animTimer->stop();
    m_hover = false;
    m_dragging = false;
    m_hoverBtn = Btn::None;
    m_btnOpacity = 0.0;
}

QRectF LiveOverlay::cardRect() const
{
    return QRectF((width() - m_cardW) / 2.0, cardTop(m_cardH), m_cardW, m_cardH);
}

bool LiveOverlay::isSettled() const
{
    return std::abs(m_cardW - m_targetW) < 1.0 && std::abs(m_cardH - m_targetH) < 1.0
        && std::abs(m_scroll - m_targetScroll) < 1.0
        && std::abs(m_capT - (hasCaption() ? 1.0 : 0.0)) < 0.01;
}

void LiveOverlay::setCaptionModel(LiveCaptionModel *model)
{
    if (m_caption) disconnect(m_caption, nullptr, this, nullptr);
    m_caption = model;
    if (m_caption) {
        connect(m_caption, &LiveCaptionModel::changed, this, &LiveOverlay::relayoutCaption);
    }
    relayoutCaption();
}

bool LiveOverlay::hasCaption() const
{
    return m_caption && !m_caption->isEmpty();
}

void LiveOverlay::relayoutCaption()
{
    // 尺寸没变时动画量不动、tick 不重绘，旧字幕会残留，所以内容一变就主动重绘
    update();
    if (!hasCaption()) {
        // 文档保留不动，收回胶囊的过程中文字还要淡出
        m_targetW = kBallSize;
        m_targetH = kPillHeight;
        m_targetScroll = 0;
        return;
    }

    QScreen *screen = targetScreen();
    const qreal screenW = screen ? screen->availableGeometry().width() : 1280;
    const qreal maxCardW = std::clamp(screenW * 0.26, 300.0, 440.0);
    const qreal maxInner = maxCardW - 2 * kPadH;

    QFont font = QGuiApplication::font();
    font.setPointSizeF(m_fontPt);

    const OverlayStyle &st = styleFor(m_theme);
    QString html;
    for (const auto &seg : m_caption->segments()) {
        html += QStringLiteral("<span style=\"color:%1\">%2</span>")
                    .arg((seg.isFinal ? st.finalText : st.pendingText).name(), seg.text.toHtmlEscaped());
    }

    m_doc.setDefaultFont(font);
    m_doc.setDocumentMargin(0);
    m_doc.setTextWidth(maxInner);
    m_doc.setHtml(html);
    m_doc.setTextWidth(maxInner);
    const qreal ideal = std::min(m_doc.idealWidth(), maxInner);
    m_doc.setTextWidth(std::ceil(ideal) + 2);

    const qreal docH = m_doc.size().height();
    qreal lineH = QFontMetricsF(font).lineSpacing();
    if (QTextLayout *layout = m_doc.firstBlock().layout(); layout && layout->lineCount() > 0) {
        lineH = layout->lineAt(0).height();
    }
    const qreal textH = std::min(docH, lineH * 2.0);
    m_targetScroll = textH - docH;

    m_targetW = std::max<qreal>(kMinCardWidth, m_doc.textWidth() + 2 * kPadH);
    m_targetH = kPadTop + textH + kTextBarsGap + kBarsRowHeight + kPadBottom;
}

void LiveOverlay::syncWindowSize()
{
    // 动画中窗口取当前和目标的较大者，落定后一次性收缩；按 8 对齐，避免每帧都改窗口大小
    auto quantize = [](qreal v, qreal floorV) { return v <= floorV ? int(floorV) : int(std::ceil((v + 4) / 8.0) * 8); };
    const qreal needW = std::max(m_cardW, m_targetW) + 2 * kSideGutter;
    const qreal needH = std::max(m_cardH, m_targetH) + kEdgeInset + 4;

    const int minW = kBallSize + 2 * kSideGutter;
    int w = std::max(width(), quantize(needW, minW));
    int h = std::max(height(), quantize(needH, kBallSize));
    if (isSettled()) {
        w = quantize(m_targetW + 2 * kSideGutter, minW);
        h = quantize(m_targetH + kEdgeInset + 4, kBallSize);
    }
    if (w != width() || h != height()) {
        resize(w, h);
        positionOnScreen();
    }
}

void LiveOverlay::tick()
{
    const qreal dt = std::min<qreal>(m_clock.restart() / 1000.0, 0.1);

    bool moving = false;
    auto step = [&moving](qreal &v, qreal next) {
        if (std::abs(next - v) > 1e-3) moving = true;
        v = next;
    };

    const qreal levelTarget = (m_state == Paused) ? 0.0 : m_level;
    step(m_smoothedLevel, approach(m_smoothedLevel, levelTarget, 0.07, dt));
    const qreal scaleTarget = 0.7 + std::pow(m_smoothedLevel, 1.3) * 0.3;
    step(m_scale, approach(m_scale, scaleTarget, 0.025, dt));

    for (int i = 0; i < kBarCount; ++i) {
        const qreal target = i < m_levels.size() ? m_levels[i] : 0.0;
        step(m_barEnergy[i], approach(m_barEnergy[i], target, 0.025, dt));
    }

    step(m_loadingOpacity, fade(m_loadingOpacity, m_state == Loading, 0.15, dt));
    step(m_transcribeOpacity, fade(m_transcribeOpacity, m_state == Transcribe, 0.10, dt));
    step(m_spectrumOpacity, fade(m_spectrumOpacity, m_state == Listening || m_state == Paused, 0.45, dt));

    step(m_cardW, approach(m_cardW, m_targetW, 0.07, dt));
    step(m_cardH, approach(m_cardH, m_targetH, 0.07, dt));
    step(m_scroll, approach(m_scroll, m_targetScroll, 0.09, dt));
    step(m_capT, fade(m_capT, hasCaption(), 0.22, dt));

    m_dotsMs = std::fmod(m_dotsMs + dt * 1000.0, kDotCycleMs);
    if (std::max(m_loadingOpacity, m_transcribeOpacity) * (1.0 - m_capT) > 0.01) moving = true;

    if (m_iconHoldSec > 0) {
        m_iconHoldSec -= dt;
    }
    step(m_iconOpacity, fade(m_iconOpacity, m_iconHoldSec > 0, 0.3, dt));

    step(m_btnOpacity, fade(m_btnOpacity, m_hover || m_dragging, 0.15, dt));

    syncWindowSize();
    updateInputMask();
    if (moving) update();
}

void LiveOverlay::updateInputMask()
{
    QRect r = cardRect().toAlignedRect();
    if (m_hover || m_dragging || m_btnOpacity > 0.01) {
        r = r.united(buttonRect(Btn::Drag).toAlignedRect());
        if (m_hideButtonEnabled) r = r.united(buttonRect(Btn::Hide).toAlignedRect());
        r.adjust(-2, -2, 2, 2);
    }
    r = r.intersected(rect());
    if (r == m_lastMaskRect) return;
    m_lastMaskRect = r;
    setMask(QRegion(r));
}

bool LiveOverlay::buttonsVertical() const
{
    return cardRect().height() >= kBtnVerticalNeed;
}

QRectF LiveOverlay::buttonRect(Btn b) const
{
    const QRectF card = cardRect();
    const int index = (b == Btn::Hide || !m_hideButtonEnabled) ? 0 : 1;
    const int count = m_hideButtonEnabled ? 2 : 1;
    const qreal span = count * kBtnSize + (count - 1) * kBtnGap;
    const qreal x0 = card.right() + kBtnOffset;
    if (buttonsVertical()) {
        const qreal y0 = card.center().y() - span / 2.0;
        return QRectF(x0, y0 + index * (kBtnSize + kBtnGap), kBtnSize, kBtnSize);
    }
    return QRectF(x0 + index * (kBtnSize + kBtnGap), card.center().y() - kBtnSize / 2.0, kBtnSize, kBtnSize);
}

LiveOverlay::Btn LiveOverlay::buttonAt(const QPointF &pos) const
{
    if (m_btnOpacity < 0.3) return Btn::None;
    if (m_hideButtonEnabled && buttonRect(Btn::Hide).contains(pos)) return Btn::Hide;
    if (buttonRect(Btn::Drag).contains(pos)) return Btn::Drag;
    return Btn::None;
}

QPoint LiveOverlay::currentAnchor() const
{
    return QPoint(x() + width() / 2, m_atTop ? y() + int(kEdgeInset) : y() + height() - int(kEdgeInset));
}

void LiveOverlay::loadSavedPosition()
{
    m_anchor.reset();
    const auto saved = FloatingWindow::loadPosition(kPosGroup);
    if (!saved) return;
    for (const QRect &s : FloatingWindow::availableScreens()) {
        if (s.contains(*saved)) { m_anchor = saved; return; }   // 屏幕拔掉了就回默认位置
    }
}

void LiveOverlay::restoreFromUserHide()
{
    m_userHidden = false;
}

void LiveOverlay::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_hover = true;
    update();
}

void LiveOverlay::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hover = false;
    m_hoverBtn = Btn::None;
    if (!m_dragging) setCursor(Qt::PointingHandCursor);
    update();
}

void LiveOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        m_anchor = m_dragStartAnchor + (event->globalPosition().toPoint() - m_dragStartGlobal);
        positionOnScreen();
        return;
    }
    const Btn b = buttonAt(event->position());
    if (b != m_hoverBtn) {
        m_hoverBtn = b;
        setCursor(b == Btn::Drag ? Qt::SizeAllCursor : Qt::PointingHandCursor);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void LiveOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        m_anchor = currentAnchor();
        FloatingWindow::savePosition(kPosGroup, *m_anchor);
        setCursor(m_hoverBtn == Btn::Drag ? Qt::SizeAllCursor : Qt::PointingHandCursor);
        return;
    }
    if (event->button() == Qt::LeftButton && m_pressBtn == Btn::Hide && buttonAt(event->position()) == Btn::Hide) {
        m_pressBtn = Btn::None;
        m_userHidden = true;
        m_hover = false;
        m_btnOpacity = 0.0;
        hide();
        emit hiddenByUser();
        return;
    }
    m_pressBtn = Btn::None;
    QWidget::mouseReleaseEvent(event);
}

void LiveOverlay::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && buttonAt(event->position()) == Btn::Drag) {
        m_dragging = false;
        m_anchor.reset();
        FloatingWindow::clearPosition(kPosGroup);
        positionOnScreen();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void LiveOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressBtn = buttonAt(event->position());
        if (m_pressBtn == Btn::Drag) {
            m_dragging = true;
            m_dragStartGlobal = event->globalPosition().toPoint();
            m_dragStartAnchor = currentAnchor();
            setCursor(Qt::ClosedHandCursor);
            return;
        }
        if (m_pressBtn == Btn::Hide) return;
    }
    if (event->button() != Qt::LeftButton || !cardRect().contains(event->position())) {
        QWidget::mousePressEvent(event);
        return;
    }
    emit sphereClicked();
    if (m_state == Paused) {
        m_icon = Icon::Pause;
    } else if (m_state == Listening) {
        m_icon = Icon::Play;
    } else {
        return;
    }
    m_iconHoldSec = kIconHoldSec;
}

void LiveOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF card = cardRect();
    const qreal t = m_capT;
    const OverlayStyle &st = styleFor(m_theme);

    const qreal cardOpacity = std::max({m_spectrumOpacity, m_loadingOpacity, m_transcribeOpacity, t});
    if (cardOpacity > 0.01) {
        paintCard(p, card.center(), cardOpacity);
    }

    const QPointF pillCenter(width() / 2.0, cardTop(kPillHeight) + kPillHeight / 2.0);
    paintDots(p, pillCenter, st.barLoading, m_loadingOpacity * (1.0 - t));
    paintDots(p, pillCenter, st.barTranscribe, m_transcribeOpacity * (1.0 - t));
    if (m_iconOpacity > 0.01) {
        paintIcon(p, card.center());
    }
    if (m_btnOpacity > 0.01) paintButtons(p);
}

QImage LiveOverlay::buttonIcon(Btn b, const QColor &color, int px) const
{
    const QString name = b == Btn::Hide ? QStringLiteral(":/overlay_hide") : QStringLiteral(":/overlay_drag");
    const QString key = name + color.name(QColor::HexArgb) + QString::number(px);
    auto it = m_iconCache.find(key);
    if (it == m_iconCache.end()) it = m_iconCache.insert(key, ToolIcon::render(name, color, px));
    return it.value();
}

void LiveOverlay::paintButtons(QPainter &p) const
{
    const OverlayStyle &st = styleFor(m_theme);
    const qreal dpr = devicePixelRatioF();
    const Btn list[] = { Btn::Hide, Btn::Drag };
    p.save();
    p.setOpacity(m_btnOpacity);
    for (Btn b : list) {
        if (b == Btn::Hide && !m_hideButtonEnabled) continue;
        const QRectF r = buttonRect(b);
        const bool hot = (m_hoverBtn == b) || (m_dragging && b == Btn::Drag);
        QColor fill = st.bgTop;
        fill.setAlpha(255);
        p.setPen(QPen(st.border, 1));
        p.setBrush(hot ? mix(fill, st.finalText, 0.16) : fill);
        p.drawRoundedRect(r, 8, 8);
        const qreal icon = 14.0;
        const QImage img = buttonIcon(b, hot ? st.finalText : st.pendingText, int(std::ceil(icon * dpr)));
        if (img.isNull()) continue;
        p.drawImage(QRectF(r.center().x() - icon / 2, r.center().y() - icon / 2, icon, icon), img);
    }
    p.restore();
}

void LiveOverlay::paintCard(QPainter &p, const QPointF &center, qreal opacity) const
{
    const qreal t = m_capT;
    const qreal level = (m_state == Paused) ? 0.0 : m_level;
    const qreal s = lerp(m_scale, 1.0, t);
    const qreal w = m_cardW * s, h = m_cardH * s;
    const QRectF r(center.x() - w / 2.0, center.y() - h / 2.0, w, h);

    p.save();
    p.setOpacity(opacity * lerp(0.8 + level * 0.2, 1.0, t));

    const OverlayStyle &st = styleFor(m_theme);
    QLinearGradient grad(r.topLeft(), r.bottomLeft());
    grad.setColorAt(0.0, st.bgTop);
    grad.setColorAt(1.0, st.bgBottom);
    p.setPen(QPen(st.border, 1));
    p.setBrush(grad);
    const qreal radius = std::min(h / 2.0, lerp(30.0, 20.0, t));
    p.drawRoundedRect(r, radius, radius);

    if (t < 0.99) {
        p.save();
        p.translate(center);
        p.scale(s, s);
        paintPillBars(p, (1.0 - t) * (opacity > 0.001 ? m_spectrumOpacity / opacity : 0.0));
        p.restore();
    }
    if (t > 0.01) {
        paintCaptionContent(p, r, t);
    }
    p.restore();
}

void LiveOverlay::paintPillBars(QPainter &p, qreal opacity) const
{
    p.save();
    p.setOpacity(p.opacity() * opacity);
    const qreal contentWidth = kBallSize - kBarMargin * 2;
    const qreal envelopeR = contentWidth / 2.0;
    const qreal barW = std::max(2.0, (contentWidth - (kBarCount - 1) * kBarSpacing) / kBarCount);
    const qreal total = kBarCount * barW + (kBarCount - 1) * kBarSpacing;
    const qreal x0 = -total / 2.0;

    p.setPen(Qt::NoPen);
    for (int i = 0; i < kBarCount; ++i) {
        const qreal x = x0 + i * (barW + kBarSpacing);
        const qreal rel = std::abs(x + barW / 2.0);
        const qreal circle = rel >= envelopeR ? 0.0
            : std::sqrt(envelopeR * envelopeR - rel * rel) / envelopeR;
        const qreal h = kMinBarHeight + std::max(0.0, m_barEnergy[i] * circle) * (envelopeR - kMinBarHeight);
        const QRectF bar = QRectF(x, -h / 2.0, barW, h).adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal rad = std::min(bar.width(), bar.height()) / 2.0;
        p.setBrush(accentAt(i / (kBarCount - 1.0)));
        p.drawRoundedRect(bar, rad, rad);
    }
    p.restore();
}

void LiveOverlay::paintCaptionContent(QPainter &p, const QRectF &card, qreal opacity) const
{
    p.save();
    p.setOpacity(p.opacity() * opacity);

    const qreal textH = card.height() - kPadTop - kTextBarsGap - kBarsRowHeight - kPadBottom;
    if (textH > 1) {
        const QRectF textRect(card.left() + kPadH, card.top() + kPadTop, card.width() - 2 * kPadH, textH);
        p.save();
        p.setClipRect(textRect);
        p.translate(textRect.left(), textRect.top() + m_scroll);
        const_cast<QTextDocument &>(m_doc).drawContents(&p);
        p.restore();
    }

    const OverlayStyle &st = styleFor(m_theme);
    p.setPen(Qt::NoPen);
    constexpr qreal dotW = 4.0, gap = 3.5;
    const qreal rowW = card.width() - 2 * kPadH;
    const int n = std::max(8, int((rowW + gap) / (dotW + gap)));
    const qreal rowLeft = card.left() + (card.width() - (n * dotW + (n - 1) * gap)) / 2.0;
    const qreal cy = card.bottom() - kPadBottom - kBarsRowHeight / 2.0;
    const qreal level = (m_state == Paused) ? 0.0 : m_level;
    for (int i = 0; i < n; ++i) {
        const qreal pos = i * (kBarCount - 1.0) / (n - 1);
        const int i0 = std::clamp(int(pos), 0, kBarCount - 1), i1 = std::min(i0 + 1, kBarCount - 1);
        const qreal e = lerp(m_barEnergy[i0], m_barEnergy[i1], pos - i0);
        const qreal energy = std::clamp(e * 0.8 + level * 0.2, 0.0, 1.0);
        const qreal h = dotW + energy * (kBarsRowHeight - dotW);

        QColor dot = accentAt(double(i) / (n - 1));
        if (m_state == Loading) dot = st.barLoading;
        else if (m_state == Transcribe) dot = st.barTranscribe;
        else if (m_state == Paused) dot = st.barPaused;
        p.setBrush(dot);
        p.drawRoundedRect(QRectF(rowLeft + i * (dotW + gap), cy - h / 2.0, dotW, h), dotW / 2.0, dotW / 2.0);
    }
    p.restore();
}

void LiveOverlay::paintDots(QPainter &p, const QPointF &center, const QColor &color, qreal opacity) const
{
    if (opacity <= 0.01) return;
    p.save();
    p.setPen(Qt::NoPen);
    constexpr qreal kDot = 10.0;
    constexpr qreal kGap = 10.0;
    const qreal x0 = center.x() - (3 * kDot + 2 * kGap) / 2.0;
    for (int i = 0; i < 3; ++i) {
        p.setOpacity(opacity * dotOpacity(i, m_dotsMs));
        p.setBrush(color);
        p.drawEllipse(QRectF(x0 + i * (kDot + kGap), center.y() - kDot / 2.0, kDot, kDot));
    }
    p.restore();
}

void LiveOverlay::paintIcon(QPainter &p, const QPointF &c) const
{
    p.save();
    p.setOpacity(m_iconOpacity);
    p.setPen(QPen(styleFor(m_theme).finalText, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if (m_icon == Icon::Pause) {
        p.drawLine(QPointF(c.x() - 5, c.y() - 16), QPointF(c.x() - 5, c.y() + 16));
        p.drawLine(QPointF(c.x() + 5, c.y() - 16), QPointF(c.x() + 5, c.y() + 16));
    } else if (m_icon == Icon::Play) {
        QPainterPath chevron;
        chevron.moveTo(c.x() - 5, c.y() - 8);
        chevron.lineTo(c.x() + 6, c.y());
        chevron.lineTo(c.x() - 5, c.y() + 8);
        p.drawPath(chevron);
    }
    p.restore();
}
