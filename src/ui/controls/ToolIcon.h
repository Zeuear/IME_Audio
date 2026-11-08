#pragma once
#include <QColor>
#include <QImage>
#include <QPalette>
#include <QString>

// @brief 横条图标取色与渲染：线条 SVG 的 currentColor 换成目标色，主题和状态只靠调色板。
namespace ToolIcon {

enum class State {
    Off,        // 开关关闭
    On,         // 开关打开（主题强调色）
    Normal,
    Record,     // 录音红，固定色
    Disabled,
};

QColor stateColor(const QPalette& palette, State state);

// path 可以是资源路径或文件路径；读不到或不是有效 SVG 时返回空图
QImage render(const QString& path, const QColor& color, int size);

} // namespace ToolIcon
