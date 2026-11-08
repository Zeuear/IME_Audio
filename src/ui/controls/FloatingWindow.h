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

// 开关置顶。改标志会让 Qt 隐藏并重建原生窗口，所以原来可见的仍显示（不激活）、位置不变
void setAlwaysOnTop(QWidget* window, bool on);

// 决定窗口左上角：无保存位置或屏幕已不存在时放主屏上方居中，否则收进窗口中心所在屏幕
QPoint place(const std::optional<QPoint>& saved, const QSize& size,
             const QList<QRect>& screens, const QRect& primary);

// 面板摆在锚点正下方左对齐，右溢往里收，下方放不下放上方；以锚点中心所在屏幕为准，不在任何屏幕内用主屏
QPoint placeBelow(const QRect& anchor, const QSize& size,
                  const QList<QRect>& screens, const QRect& primary);

// 当前真实屏幕的可用区域，给 place / placeBelow 用
QList<QRect> availableScreens();
QRect primaryScreenArea();

// 位置和大小存在 ini 的 group 分组下，不动 AppConfig；没存过返回空
std::optional<QPoint> loadPosition(const QString& group);
void savePosition(const QString& group, const QPoint& pos);
std::optional<QSize> loadSize(const QString& group);
void saveSize(const QString& group, const QSize& size);

// 按当前真实屏幕摆放窗口：读取保存的位置，经 place 校正后 move
void restore(QWidget* window, const QString& group);

} // namespace FloatingWindow
