#include "SnippetSuggestBar.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QScreen>

namespace {

constexpr int kAutoHideMs = 8000;
constexpr int kMaxLabelWidth = 160;
constexpr int kGapToAnchor = 8;

} // namespace

SnippetSuggestBar::SnippetSuggestBar(QWidget *parent) : QWidget(parent) {
    // 和 LiveOverlay 一样：不能成为焦点窗口，否则目标应用失去前台，粘贴就落空
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
#endif

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(10, 8, 10, 8);
    m_layout->setSpacing(6);

    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(kAutoHideMs);
    connect(&m_hideTimer, &QTimer::timeout, this, &SnippetSuggestBar::closeAndRemember);
}

void SnippetSuggestBar::setSuggestions(const QVector<Suggestion>& suggestions) {
    QStringList ids;
    for (const Suggestion& s : suggestions) ids << s.id;
    if (ids.isEmpty()) {
        dismiss();
        return;
    }
    if (ids == m_currentIds && isVisible()) return;
    m_currentIds = ids;

    while (QLayoutItem* item = m_layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const QFontMetrics fm(font());
    for (const Suggestion& s : suggestions) {
        auto* button = new QPushButton(fm.elidedText(s.label, Qt::ElideRight, kMaxLabelWidth), this);
        button->setToolTip(s.tooltip);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        const QString id = s.id;
        connect(button, &QPushButton::clicked, this, [this, id]() {
            emit snippetClicked(id);
            closeAndRemember();
        });
        m_layout->addWidget(button);
    }
    adjustSize();
    show();
    m_hideTimer.start();
}

void SnippetSuggestBar::placeNear(const QRect& anchor, bool below) {
    QScreen* screen = QGuiApplication::screenAt(anchor.center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QRect area = screen->availableGeometry();

    int x = anchor.center().x() - width() / 2;
    int y = below ? anchor.bottom() + kGapToAnchor : anchor.top() - height() - kGapToAnchor;
    x = qBound(area.left(), x, area.right() - width());
    y = qBound(area.top(), y, area.bottom() - height());
    move(x, y);
}

void SnippetSuggestBar::closeAndRemember() {
    const QStringList ids = m_currentIds;
    dismiss();
    if (!ids.isEmpty()) emit closed(ids);
}

void SnippetSuggestBar::dismiss() {
    m_hideTimer.stop();
    m_currentIds.clear();
    hide();
}

void SnippetSuggestBar::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette& pal = palette();
    p.setBrush(pal.color(QPalette::Window));
    p.setPen(pal.color(QPalette::Mid));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
}

void SnippetSuggestBar::enterEvent(QEnterEvent *event) {
    m_hideTimer.stop();   // 鼠标在上面时不自动消失
    QWidget::enterEvent(event);
}

void SnippetSuggestBar::leaveEvent(QEvent *event) {
    if (isVisible()) m_hideTimer.start();
    QWidget::leaveEvent(event);
}
