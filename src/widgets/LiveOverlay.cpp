#include "LiveOverlay.h"
#include "LiveCaptionModel.h"
#include <QFontMetricsF>
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
constexpr qreal kBottomInset = 30.0;   // 胶囊底边距窗口底部，保持原 120x120 窗口里的位置
constexpr qreal kPadH = 18.0, kPadTop = 12.0, kPadBottom = 12.0, kTextBarsGap = 8.0;
constexpr qreal kBarsRowHeight = 14.0;
constexpr qreal kMinCardWidth = 220.0;
constexpr int kBarCount = 16;
constexpr qreal kBarSpacing = 1.0;
constexpr qreal kBarMargin = 12.0;
constexpr qreal kMinBarHeight = 5.0;
constexpr qreal kDotCycleMs = 1120.0;
constexpr qreal kIconHoldSec = 1.5;

qreal approach(qreal cur, qreal target, qreal tauSec, qreal dtSec)
{
    return target + (cur - target) * std::exp(-dtSec / tauSec);
}

// sec：从 0 到 1 所需时间
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

// 底色在胶囊和卡片形态下一致，变形时只有尺寸变化；频谱始终用蓝紫点缀色
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
} // namespace

LiveOverlay::LiveOverlay(QWidget *parent)
    : QWidget(parent)
{
    // 只做状态显示，不能成为焦点窗口，否则听写结果会注入到它自己身上
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    // macOS 上 Tool 窗口在应用失去焦点时会被系统隐藏，而录音时焦点在目标应用里
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
#endif
    setCursor(Qt::PointingHandCursor);
    resize(kBallSize, kBallSize);

    m_barEnergy.fill(0.0, kBarCount);

    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(16);
    connect(m_animTimer, &QTimer::timeout, this, &LiveOverlay::tick);

    m_idleFadeTimer = new QTimer(this);
    m_idleFadeTimer->setSingleShot(true);
    m_idleFadeTimer->setInterval(4000);
    connect(m_idleFadeTimer, &QTimer::timeout, this, [this]() {
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

void LiveOverlay::setState(State s) { m_state = s; }
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

void LiveOverlay::positionBottomCenter()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    QRect geo = screen->availableGeometry();
    constexpr int margin = -5;
    move(geo.center().x() - width() / 2, geo.bottom() - height() - margin);
}

void LiveOverlay::showAtBottomCenter()
{
    positionBottomCenter();
    show();
    // macOS 上 raise() 会抢走用户正在输入的窗口焦点
#ifndef Q_OS_MACOS
    raise();
#endif
}

void LiveOverlay::hideOverlay() { hide(); }
void LiveOverlay::hideTimerStart() { m_idleFadeTimer->start(); }
void LiveOverlay::hideTimerStop() { m_idleFadeTimer->stop(); }

void LiveOverlay::onVoiceStarted()
{
    m_idleFadeTimer->stop();
    setState(Listening);
    showAtBottomCenter();
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
}

QRectF LiveOverlay::cardRect() const
{
    // 底边固定、水平居中：文字变多只向上长，频谱位置不动
    return QRectF((width() - m_cardW) / 2.0, height() - kBottomInset - m_cardH, m_cardW, m_cardH);
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
    if (!hasCaption()) {
        // 文档保留不动，收回胶囊的过程中文字还要淡出
        m_targetW = kBallSize;
        m_targetH = kPillHeight;
        m_targetScroll = 0;
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    const qreal screenW = screen ? screen->availableGeometry().width() : 1280;
    const qreal maxCardW = std::clamp(screenW * 0.26, 300.0, 440.0);
    const qreal maxInner = maxCardW - 2 * kPadH;

    QFont font = QGuiApplication::font();
    font.setPointSizeF(font.pointSizeF() + 3);

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
    // 按目标宽度换行：卡片宽度在动画中逐步变宽时，换行位置不会跟着来回跳
    m_doc.setTextWidth(std::ceil(ideal) + 2);

    // 行高取排版结果而不是字体度量，两者差几个像素会漏出第三行的下伸部分
    const qreal docH = m_doc.size().height();
    qreal lineH = QFontMetricsF(font).lineSpacing();
    if (QTextLayout *layout = m_doc.firstBlock().layout(); layout && layout->lineCount() > 0) {
        lineH = layout->lineAt(0).height();
    }
    // 最多露出两行，滚动量让最新的一行始终贴在底部
    const qreal textH = std::min(docH, lineH * 2.0);
    m_targetScroll = textH - docH;

    m_targetW = std::max<qreal>(kMinCardWidth, m_doc.textWidth() + 2 * kPadH);
    m_targetH = kPadTop + textH + kTextBarsGap + kBarsRowHeight + kPadBottom;
}

void LiveOverlay::syncWindowSize()
{
    // 动画中窗口取当前和目标的较大者，落定后一次性收缩；按 8 对齐，避免每帧都改窗口大小
    auto quantize = [](qreal v, qreal floorV) { return v <= floorV ? int(floorV) : int(std::ceil((v + 4) / 8.0) * 8); };
    const qreal needW = std::max(m_cardW, m_targetW);
    const qreal needH = std::max(m_cardH, m_targetH) + kBottomInset + 4;

    int w = std::max(width(), quantize(needW, kBallSize));
    int h = std::max(height(), quantize(needH, kBallSize));
    if (isSettled()) {
        w = quantize(m_targetW, kBallSize);
        h = quantize(m_targetH + kBottomInset + 4, kBallSize);
    }
    if (w != width() || h != height()) {
        resize(w, h);
        positionBottomCenter();
    }
}

void LiveOverlay::tick()
{
    const qreal dt = std::min<qreal>(m_clock.restart() / 1000.0, 0.1);

    const qreal levelTarget = (m_state == Paused) ? 0.0 : m_level;
    m_smoothedLevel = approach(m_smoothedLevel, levelTarget, 0.07, dt);
    const qreal scaleTarget = 0.7 + std::pow(m_smoothedLevel, 1.3) * 0.3;
    m_scale = approach(m_scale, scaleTarget, 0.025, dt);

    for (int i = 0; i < kBarCount; ++i) {
        const qreal target = i < m_levels.size() ? m_levels[i] : 0.0;
        m_barEnergy[i] = approach(m_barEnergy[i], target, 0.025, dt);
    }

    m_loadingOpacity = fade(m_loadingOpacity, m_state == Loading, 0.15, dt);
    m_transcribeOpacity = fade(m_transcribeOpacity, m_state == Transcribe, 0.10, dt);
    m_spectrumOpacity = fade(m_spectrumOpacity, m_state == Listening || m_state == Paused, 0.45, dt);

    m_cardW = approach(m_cardW, m_targetW, 0.07, dt);
    m_cardH = approach(m_cardH, m_targetH, 0.07, dt);
    m_scroll = approach(m_scroll, m_targetScroll, 0.09, dt);
    m_capT = fade(m_capT, hasCaption(), 0.22, dt);

    m_dotsMs = std::fmod(m_dotsMs + dt * 1000.0, kDotCycleMs);

    if (m_iconHoldSec > 0) {
        m_iconHoldSec -= dt;
    }
    m_iconOpacity = fade(m_iconOpacity, m_iconHoldSec > 0, 0.3, dt);

    syncWindowSize();
    update();
}

void LiveOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !cardRect().contains(event->position())) {
        QWidget::mousePressEvent(event);
        return;
    }
    emit sphereClicked();
    // 槽里已同步切换了 Listening / Paused：刚暂停显示暂停图标，刚恢复显示播放图标
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

    // 加载 / 转录态也画同一块底色；有字幕时转录态保留卡片，等最终结果回来
    const qreal cardOpacity = std::max({m_spectrumOpacity, m_loadingOpacity, m_transcribeOpacity, t});
    if (cardOpacity > 0.01) {
        paintCard(p, card.center(), cardOpacity);
    }

    // 卡片形态下状态由底部圆点行的颜色表达，三个点只在胶囊形态出现
    const QPointF pillCenter(width() / 2.0, height() - kBottomInset - kPillHeight / 2.0);
    paintDots(p, pillCenter, st.barLoading, m_loadingOpacity * (1.0 - t));
    paintDots(p, pillCenter, st.barTranscribe, m_transcribeOpacity * (1.0 - t));
    if (m_iconOpacity > 0.01) {
        paintIcon(p, card.center());
    }
}

void LiveOverlay::paintCard(QPainter &p, const QPointF &center, qreal opacity) const
{
    const qreal t = m_capT;
    const qreal level = (m_state == Paused) ? 0.0 : m_level;
    // 胶囊随音量缩放，变成卡片后保持原尺寸，避免文字跟着抖
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
        // 底色整体按 opacity 淡入淡出，频谱条只在聆听 / 暂停态出现，这里换算成相对底色的占比
        paintPillBars(p, (1.0 - t) * (opacity > 0.001 ? m_spectrumOpacity / opacity : 0.0));
        p.restore();
    }
    if (t > 0.01) {
        paintCaptionContent(p, r, t);
    }
    p.restore();
}

// 柱高按圆形包络缩放：中间高、两边低
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
        // 圆角半径不能超过 min(宽, 高) / 2，否则两端会变尖
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

    // 文字区高度跟着卡片当前高度走：一行变两行时，第二行随卡片长高被揭开
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
        // 16 路频谱插值到 n 个圆点，再叠加整体电平，让每个点都有起伏
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
