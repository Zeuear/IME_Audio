#pragma once

#include <QString>

// @brief
// 语种显示名：把英文名转成随界面语言翻译的名称，并带上 ISO 代码，如中文界面"法语 (fr)"、
// 英文界面"French (fr)"。语种有哪些、各有哪些模型由 ModelRegistry 的 LanguageTables 决定，
class LanguageCatalog {
public:
    static QString codeOf(const QString& key);
    // 代码反查英文名；不在目录里返回空
    static QString keyOfCode(const QString& code);         
    static QString displayName(const QString& key);
    static QString plainName(const QString& key);     
};
