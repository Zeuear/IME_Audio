#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

struct SnippetItem {
    std::string id;
    std::string keyword;      // 快捷码
    std::string title;        // A列：中文（显示用）
    std::string text;         // B列：外文（粘贴目标）
    std::string imagePath;    // 相对路径 data/images/xxx，或 http(s) 链接
    std::string type;         // text / image / combo
    int32_t useCount = 0;
};

struct FolderNode {
    std::string id;
    int32_t level = 1;        // 1=主表, 2=Sheet, 3=@@分组
    std::string title;
    bool collapsed = false;
    std::string sourceUrl;    // 仅 L1
    std::string spreadsheetId; // 仅 L1
    std::vector<std::unique_ptr<FolderNode>> children;
    std::vector<SnippetItem> items;
};

using FolderList = std::vector<std::unique_ptr<FolderNode>>;

struct AppSettings {
    std::string hotkey;   // 默认留空：Alt+Space 会和系统窗口菜单冲突，由用户自己设置
    std::vector<std::string> triggerPrefixes = { "//", ";;" };
    bool enablePrefixTrigger = true;
    bool autoDeletePrefix = true;
    bool comboImageFirst = false;
    bool multilineEnabled = true;
    int32_t maxDisplayLines = 3;
    bool voiceTrigger = true;      // 说到条目的快捷码时，在悬浮窗里提示可点击的指令
};
