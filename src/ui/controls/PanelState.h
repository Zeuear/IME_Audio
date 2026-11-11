#pragma once

// @brief 横条下方展开面板的页面状态，只有纯逻辑，不含界面代码，便于单元测试。
namespace PanelState {

enum class Page { None, Transcript, Dictionary, File };

// 点某页图标后的状态：点当前页收起，点另一页切过去，面板收着则打开该页
inline Page afterClick(Page current, Page clicked)
{
    return current == clicked ? Page::None : clicked;
}

} // namespace PanelState
