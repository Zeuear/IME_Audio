#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include <memory>
#include <cstdint>

struct SnippetItem {
    QString id;
    QString keyword;      // 快捷码
    QString title;        // A列：中文（显示用）
    QString text;         // B列：外文（粘贴目标）
    QString imagePath;    // 相对路径 data/images/xxx，或 http(s) 链接
    QString type;         // text / image / combo
    int32_t useCount = 0;
};

struct FolderNode {
    QString id;
    int32_t level = 1;        // 1=主表, 2=Sheet, 3=@@分组
    QString title;
    bool collapsed = false;
    QString sourceUrl;    // 仅 L1
    QString spreadsheetId; // 仅 L1
    std::vector<std::unique_ptr<FolderNode>> children;
    std::vector<SnippetItem> items;
};

using FolderList = std::vector<std::unique_ptr<FolderNode>>;

struct AppSettings {
    QString hotkey;   // 默认留空：Alt+Space 会和系统窗口菜单冲突，由用户自己设置
    QStringList triggerPrefixes = { "//", ";;" };
    bool enablePrefixTrigger = true;
    bool autoDeletePrefix = true;
    bool comboImageFirst = false;
    bool multilineEnabled = true;
    int32_t maxDisplayLines = 3;
    bool voiceTrigger = true;      // 说到条目的快捷码时，在悬浮窗里提示可点击的指令
};
