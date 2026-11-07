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
    QList<QRect> screens;
    for (QScreen* screen : QGuiApplication::screens()) screens << screen->availableGeometry();
    QScreen* primary = QGuiApplication::primaryScreen();
    const QRect primaryRect = primary ? primary->availableGeometry() : QRect(0, 0, 1280, 720);
    window->move(place(loadPosition(group), window->size(), screens, primaryRect));
}

} // namespace FloatingWindow
