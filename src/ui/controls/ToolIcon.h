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

// 带底板的图标，仿 macOS / iOS 的凸起圆角方块：渐变底、顶部内高光、底部内阴影、描边，
// 线条图案压一层"压印"阴影。size 是像素边长（按 28 逻辑像素的设计稿等比缩放）。
// accent 有效：彩色底 + 白色图案，用于"开启 / 进行中"；accent 无效：随主题的中性底 + glyph 色图案。
// hover 让底板略亮一点，给悬停反馈
QImage renderTile(const QString& path, int size, const QPalette& palette,
                  const QColor& accent, const QColor& glyph, bool hover);

} // namespace ToolIcon
