#pragma once
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <optional>

class QWidget;

// @brief 悬浮小窗共用的窗口标志与位置逻辑：置顶不抢焦点，位置存 ini，屏幕拔掉后回主屏。
namespace FloatingWindow {

// 置顶、无边框、透明、不接受焦点：避免点击它切走前台窗口、导致听写结果注入到自己；须在显示前调用
void applyToolWindowFlags(QWidget* window);

void setAlwaysOnTop(QWidget* window, bool on);

QPoint place(const std::optional<QPoint>& saved, const QSize& size,
             const QList<QRect>& screens, const QRect& primary);

// 面板摆在锚点正下方左对齐，右溢往里收，下方放不下放上方；以锚点中心所在屏幕为准，不在任何屏幕内用主屏
QPoint placeBelow(const QRect& anchor, const QSize& size,
                  const QList<QRect>& screens, const QRect& primary);

QList<QRect> availableScreens();
QRect primaryScreenArea();

std::optional<QPoint> loadPosition(const QString& group);
void savePosition(const QString& group, const QPoint& pos);
void clearPosition(const QString& group);
std::optional<QSize> loadSize(const QString& group);
void saveSize(const QString& group, const QSize& size);

void restore(QWidget* window, const QString& group);

}
