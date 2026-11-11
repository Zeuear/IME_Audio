#pragma once

#include <QString>

// @brief
// 语种显示名
class LanguageCatalog {
public:
    static QString codeOf(const QString& key);
    static QString keyOfCode(const QString& code);         
    static QString displayName(const QString& key);
    static QString plainName(const QString& key);     
};
