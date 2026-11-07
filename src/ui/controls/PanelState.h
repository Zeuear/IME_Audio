#pragma once

// @brief 横条下方展开面板的页面状态，只有纯逻辑，不含界面代码，便于单元测试。
namespace PanelState {

enum class Page { None, Dictionary, File };

// 点了某个页面的图标之后，面板应该是什么状态：
// 点的就是当前页 → 收起；点另一个 → 切过去；面板收着 → 打开点的那页
inline Page afterClick(Page current, Page clicked)
{
    return current == clicked ? Page::None : clicked;
}

} // namespace PanelState
