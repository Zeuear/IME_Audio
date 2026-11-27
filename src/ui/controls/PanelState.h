#pragma once

// @brief 横条下方展开面板的页面状态，只有纯逻辑，不含界面代码，便于单元测试。
namespace PanelState {

enum class Page { None, Transcript, Dictionary, File };

inline Page afterClick(Page current, Page clicked)
{
    return current == clicked ? Page::None : clicked;
}

}
