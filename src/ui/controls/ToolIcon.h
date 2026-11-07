#pragma once
#include <QColor>
#include <QImage>
#include <QPalette>
#include <QString>

// @brief 横条图标的取色与渲染。图标是统一的线条 SVG，线条写成 currentColor，
//        渲染时换成目标颜色，所以 light / gray / dark 主题和开关状态都只靠调色板，不需要多套图。
namespace ToolIcon {

enum class State {
    Off,        // 开关关闭：弱化文字色
    On,         // 开关打开：强调色
    Normal,     // 开始、停止这类动作按钮可点时：普通文字色
    Disabled,   // 不可用
};

// 颜色取自调色板，不写死，三个主题自动适配
QColor stateColor(const QPalette& palette, State state);

// path 可以是资源路径（:/tool_play）或文件路径；读不到或不是有效 SVG 时返回空图
QImage render(const QString& path, const QColor& color, int size);

} // namespace ToolIcon
