#include "BubbleTip.h"
#include <QDateTime>
#include <QEvent>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QVariantAnimation>

namespace {
constexpr int kShowDelayMs = 250;
constexpr int kChainWindowMs = 400;
constexpr int kPadX = 10;
constexpr int kPadY = 6;
constexpr int kRadius = 7;
constexpr int kArrowW = 14;
constexpr int kArrowH = 7;
constexpr int kGap = 9;
constexpr int kMaxTextWidth = 280;
constexpr int kTextFlags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap;
const QColor kFill(40, 40, 40, 240);
const QColor kBorder(255, 255, 255, 40);
}

BubbleTip::BubbleTip()
    : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);

    m_delay = new QTimer(this);
    m_delay->setSingleShot(true);
    m_delay->setInterval(kShowDelayMs);
    connect(m_delay, &QTimer::timeout, this, [this]() {
        if (m_target) showFor(m_target);
    });

    m_fade = new QVariantAnimation(this);
    m_fade->setStartValue(0.0);
    m_fade->setEndValue(1.0);
    m_fade->setDuration(120);
    connect(m_fade, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& v) { setWindowOpacity(v.toDouble()); });
}

BubbleTip* BubbleTip::instance()
{
    static BubbleTip* tip = new BubbleTip;   // 程序退出时随进程回收
    return tip;
}

void BubbleTip::install(QWidget* target)
{
    target->installEventFilter(instance());
}

bool BubbleTip::eventFilter(QObject* watched, QEvent* event)
{
    auto* w = qobject_cast<QWidget*>(watched);
    if (!w) return false;
    switch (event->type()) {
    case QEvent::ToolTip:
        return true;
    case QEvent::Enter:
        m_target = w;
        if (QDateTime::currentMSecsSinceEpoch() - m_hiddenAt < kChainWindowMs) showFor(w);
        else m_delay->start();
        break;
    case QEvent::Leave:
    case QEvent::MouseButtonPress:
    case QEvent::Hide:
        if (m_target == w) {
            hideTip();
            if (event->type() != QEvent::Leave) m_hiddenAt = 0;   // 点击后不连锁弹出
            m_target = nullptr;
        }
        break;
    case QEvent::ToolTipChange:
        if (m_target == w && isVisible()) showFor(w);
        break;
    default:
        break;
    }
    return false;
}

void BubbleTip::showFor(QWidget* target)
{
    m_delay->stop();
    m_text = target->toolTip();
    if (m_text.isEmpty() || !target->isVisible()) {
        hideTip();
        return;
    }

    // 顶层窗口第一次显示前还没套上全局样式表的字体，不先 polish 会按小字号量、按大字号画
    ensurePolished();
    const QFontMetrics fm = fontMetrics();
    int textWidth = 0;
    for (const QString& line : m_text.split(QLatin1Char('\n'))) textWidth = qMax(textWidth, fm.horizontalAdvance(line));
    textWidth = qMin(textWidth + 1, kMaxTextWidth);
    const QRect textBound = fm.boundingRect(QRect(0, 0, textWidth, INT_MAX), kTextFlags, m_text);
    m_textSize = QSize(textWidth, textBound.height());
    const QSize size(textWidth + 2 * kPadX, textBound.height() + 2 * kPadY + kArrowH);

    const QRect anchor(target->mapToGlobal(QPoint(0, 0)), target->size());
    QScreen* screen = QGuiApplication::screenAt(anchor.center());
    const QRect avail = screen ? screen->availableGeometry() : QRect(anchor.center() - QPoint(2000, 2000), QSize(4000, 4000));

    int y = anchor.top() - kGap - size.height();
    m_below = y < avail.top();
    if (m_below) y = anchor.bottom() + 1 + kGap;

    int x = anchor.center().x() - size.width() / 2;
    x = qBound(avail.left(), x, avail.right() + 1 - size.width());
    m_arrowX = qBound(kRadius + kArrowW / 2, anchor.center().x() - x, size.width() - kRadius - kArrowW / 2);

    const bool wasVisible = isVisible();
    setGeometry(QRect(QPoint(x, y), size));
    update();
    if (!wasVisible) {
        setWindowOpacity(0.0);
        show();
        m_fade->start();
    }
    // 横条也是置顶窗口，且会被反复 raise；气泡每次都顶到它上面，否则下半截被横条的阴影边盖住
    raise();
}

void BubbleTip::hideTip()
{
    m_delay->stop();
    if (!isVisible()) return;
    m_fade->stop();
    hide();
    m_hiddenAt = QDateTime::currentMSecsSinceEpoch();
}

void BubbleTip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF body = QRectF(rect()).adjusted(0.5, m_below ? kArrowH + 0.5 : 0.5,
                                                -0.5, m_below ? -0.5 : -kArrowH - 0.5);
    QPainterPath path;
    path.addRoundedRect(body, kRadius, kRadius);
    QPainterPath arrow;
    const qreal baseY = m_below ? body.top() + 1 : body.bottom() - 1;
    const qreal tipY = m_below ? 0.5 : height() - 0.5;
    arrow.moveTo(m_arrowX - kArrowW / 2.0, baseY);
    arrow.lineTo(m_arrowX, tipY);
    arrow.lineTo(m_arrowX + kArrowW / 2.0, baseY);
    arrow.closeSubpath();
    path = path.united(arrow);

    p.setPen(QPen(kBorder, 1));
    p.setBrush(kFill);
    p.drawPath(path);

    p.setPen(Qt::white);
    const int textTop = m_below ? kArrowH + kPadY : kPadY;
    p.drawText(QRect(QPoint(kPadX, textTop), m_textSize), kTextFlags, m_text);
}
