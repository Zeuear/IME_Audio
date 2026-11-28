#pragma once
#include <QList>
#include <QString>

class TermsLibraryManager;

// @brief 词典批量导入：把粘贴的文本或拖入的文件识别成「听成 → 写成」词条，再合并进某个词典。
//        不依赖界面，输入是文本，输出是词条列表加逐行问题，便于单测。
namespace GlossaryImport {

struct Entry {
    QString heard;     // 听成 / 源词；可含用逗号、斜线分隔的多个说法
    QString correct;   // 写成 / 译文
    QString notes;
    QString tags;
    bool enabled = true;
};

struct Issue {
    int line = 0;      // 从 1 开始；JSON 输入时为 0
    QString text;
    QString reason;
};

struct Parsed {
    // aurapro-glossary / json / tsv / lines
    QString format;
    QString name;
    QString version;
    QString sourceLang;
    QString glossaryLang;
    QList<Entry> entries;
    QList<Issue> issues;
};

// requireHeard：翻译词典每条都必须有源词；识别词典允许只写一个词（只做热词）
Parsed parseText(const QString& text, bool requireHeard);
Parsed parseFile(const QString& path, bool requireHeard, QString* error = nullptr);
// 读文本文件：优先按 UTF-8，不是则按系统编码（Windows 记事本存的 GBK 文件）
QString readTextFile(const QString& path, QString* error = nullptr);

// 多数条目的两栏文字系统与词典语言方向相反时返回 true，界面据此建议对调两栏
bool looksSwapped(const QList<Entry>& entries, const QString& langA, const QString& langB);

struct Summary {
    int added = 0;
    int duplicates = 0;   // 与已有词条或本批前面的词条重复，跳过
    int invalid = 0;      // 缺少必填栏，跳过
    int mismatched = 0;   // 文字系统与词典语言不符，仍然导入但不写语言标签
};

// 整批只触发一次保存与重建规则。swap：导入前先对调每条的两栏
Summary mergeInto(TermsLibraryManager& library, const QList<Entry>& entries, bool requireHeard,
                  const QString& langA, const QString& langB, bool swap);

}
