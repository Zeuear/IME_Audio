#include "FloatingWindow.h"
#include "config/ConfigManager.h"
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QWidget>
#include <algorithm>

namespace FloatingWindow {

namespace {
constexpr int kTopMargin = 12;
constexpr int kPanelGap = 6;

QPoint topCenter(const QSize& size, const QRect& primary)
{
    return QPoint(primary.left() + (primary.width() - size.width()) / 2, primary.top() + kTopMargin);
}

QSettings openSettings()
{
    // 和 ConfigManager 读写同一个 ini：QSettings 保留不认识的分组，互不覆盖
    return QSettings(ConfigManager::configFilePath(), QSettings::IniFormat);
}
} // namespace

void applyToolWindowFlags(QWidget* window)
{
    window->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                           | Qt::WindowDoesNotAcceptFocus);
    window->setAttribute(Qt::WA_TranslucentBackground);
    window->setAttribute(Qt::WA_ShowWithoutActivating);
    // macOS 上 Tool 窗口在应用失去焦点时会被系统隐藏，而录音时焦点在目标应用里
#ifdef Q_OS_MACOS
    window->setAttribute(Qt::WA_MacAlwaysShowToolWindow);
#endif
}

void setAlwaysOnTop(QWidget* window, bool on)
{
    if (bool(window->windowFlags() & Qt::WindowStaysOnTopHint) == on) return;
    const bool wasVisible = window->isVisible();
    const QPoint where = window->pos();
    window->setWindowFlag(Qt::WindowStaysOnTopHint, on);   // 会隐藏窗口
    if (wasVisible) {
        window->move(where);
        window->show();   // WA_ShowWithoutActivating 在，所以不会抢焦点
    }
}

QPoint place(const std::optional<QPoint>& saved, const QSize& size,
             const QList<QRect>& screens, const QRect& primary)
{
    if (!saved) return topCenter(size, primary);

    // 先看窗口中心在哪个屏幕；中心刚好滑出屏幕边缘时，取和窗口重叠最多的屏幕
    const QRect window(*saved, size);
    const QPoint center = window.center();
    const QRect* target = nullptr;
    qint64 bestOverlap = 0;
    for (const QRect& screen : screens) {
        if (screen.contains(center)) { target = &screen; break; }
        const QRect overlap = screen.intersected(window);
        const qint64 area = overlap.isValid() ? qint64(overlap.width()) * overlap.height() : 0;
        if (area > bestOverlap) { bestOverlap = area; target = &screen; }
    }
    if (!target) return topCenter(size, primary);   // 之前所在的屏幕已经拔掉，或分辨率变小后完全在屏外

    // 窗口比屏幕还大时以左上角为准，不让 max 小于 min
    const int maxX = std::max(target->left(), target->right() - size.width() + 1);
    const int maxY = std::max(target->top(), target->bottom() - size.height() + 1);
    return QPoint(std::clamp(saved->x(), target->left(), maxX),
                  std::clamp(saved->y(), target->top(), maxY));
}

QPoint placeBelow(const QRect& anchor, const QSize& size,
                  const QList<QRect>& screens, const QRect& primary)
{
    QRect target = primary;
    for (const QRect& screen : screens) {
        if (screen.contains(anchor.center())) { target = screen; break; }
    }

    const int maxX = std::max(target.left(), target.right() - size.width() + 1);
    const int x = std::clamp(anchor.left(), target.left(), maxX);

    const int below = anchor.bottom() + 1 + kPanelGap;
    if (below + size.height() - 1 <= target.bottom()) return QPoint(x, below);

    const int above = anchor.top() - kPanelGap - size.height();
    if (above >= target.top()) return QPoint(x, above);

    // 上下都放不下（屏幕很矮）：贴着屏幕下沿，至少保证面板在屏幕内
    return QPoint(x, std::max(target.top(), target.bottom() - size.height() + 1));
}

QList<QRect> availableScreens()
{
    QList<QRect> screens;
    for (QScreen* screen : QGuiApplication::screens()) screens << screen->availableGeometry();
    return screens;
}

QRect primaryScreenArea()
{
    QScreen* primary = QGuiApplication::primaryScreen();
    return primary ? primary->availableGeometry() : QRect(0, 0, 1280, 720);
}

std::optional<QPoint> loadPosition(const QString& group)
{
    QSettings s = openSettings();
    s.beginGroup(group);
    if (!s.contains(QStringLiteral("x")) || !s.contains(QStringLiteral("y"))) return std::nullopt;
    return QPoint(s.value(QStringLiteral("x")).toInt(), s.value(QStringLiteral("y")).toInt());
}

void savePosition(const QString& group, const QPoint& pos)
{
    QSettings s = openSettings();
    s.beginGroup(group);
    s.setValue(QStringLiteral("x"), pos.x());
    s.setValue(QStringLiteral("y"), pos.y());
}

std::optional<QSize> loadSize(const QString& group)
{
    QSettings s = openSettings();
    s.beginGroup(group);
    if (!s.contains(QStringLiteral("w")) || !s.contains(QStringLiteral("h"))) return std::nullopt;
    const QSize size(s.value(QStringLiteral("w")).toInt(), s.value(QStringLiteral("h")).toInt());
    if (size.width() <= 0 || size.height() <= 0) return std::nullopt;   // 坏数据当作没存过
    return size;
}

void saveSize(const QString& group, const QSize& size)
{
    QSettings s = openSettings();
    s.beginGroup(group);
    s.setValue(QStringLiteral("w"), size.width());
    s.setValue(QStringLiteral("h"), size.height());
}

void restore(QWidget* window, const QString& group)
{
    window->move(place(loadPosition(group), window->size(), availableScreens(), primaryScreenArea()));
}

} // namespace FloatingWindow
