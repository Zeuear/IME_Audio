#pragma once
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <optional>

class QWidget;

// @brief 悬浮小窗（LiveOverlay、横条）共用的窗口标志与位置逻辑：
//        置顶且不抢焦点，位置记在 ini 里，下次启动回到原处，屏幕拔掉后回到主屏。
namespace FloatingWindow {

// 置顶、无边框、透明背景、不接受焦点：点击它不会把前台窗口切走，
// 否则听写结果会注入到它自己身上。必须在窗口显示之前调用
void applyToolWindowFlags(QWidget* window);

// 开关"始终置顶"。改窗口标志会让 Qt 先隐藏窗口、重建原生窗口，所以这里负责：
// 原来可见的仍然显示（不激活，不抢焦点）、位置不变，"不接受焦点"等其他标志保持不变
void setAlwaysOnTop(QWidget* window, bool on);

// 决定窗口左上角：没有保存过位置或所在屏幕已不存在时，放主屏上方居中；
// 否则整个收进窗口中心所在的屏幕。screens 是各屏的可用区域
QPoint place(const std::optional<QPoint>& saved, const QSize& size,
             const QList<QRect>& screens, const QRect& primary);

// 面板摆在锚点（横条）正下方、左边对齐，间隔几个像素；右边超出时往里收；
// 下方放不下就放到锚点上方。以锚点中心所在的屏幕为准，不在任何屏幕内时用主屏
QPoint placeBelow(const QRect& anchor, const QSize& size,
                  const QList<QRect>& screens, const QRect& primary);

// 当前真实屏幕的可用区域，给 place / placeBelow 用
QList<QRect> availableScreens();
QRect primaryScreenArea();

// 位置和大小存在配置文件的 group 分组下，不动 AppConfig。
// 各窗口用各自的分组，和 ConfigManager 共用同一个 ini 互不覆盖；没存过时返回空
std::optional<QPoint> loadPosition(const QString& group);
void savePosition(const QString& group, const QPoint& pos);
std::optional<QSize> loadSize(const QString& group);
void saveSize(const QString& group, const QSize& size);

// 按当前真实屏幕摆放窗口：读取保存的位置，经 place 校正后 move
void restore(QWidget* window, const QString& group);

} // namespace FloatingWindow
