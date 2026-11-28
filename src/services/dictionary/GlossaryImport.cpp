#include "GlossaryImport.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStringConverter>
#include <QStringList>
#include <QVector>
#include <utility>

#include "EntryCheck.h"
#include "services/TermsLibraryManager.h"

namespace GlossaryImport {

namespace {

// 常见字段名；JSON 数组里的对象按这些名字挑出两栏
const QStringList kHeardKeys = { "heard", "wrong", "source", "src", "from", "term", "original", "key",
                                 "原文", "听成", "源词", "术语", "源" };
const QStringList kCorrectKeys = { "correct", "target", "translation", "to", "value", "译文", "写成", "翻译", "目标" };
const QStringList kNoteKeys = { "notes", "note", "remark", "备注" };

QString stripQuotes(QString s) {
    s = s.trimmed();
    if (s.size() >= 2) {
        const QChar a = s.front();
        const QChar b = s.back();
        const bool quoted = (a == '"' && b == '"') || (a == '\'' && b == '\'')
                            || (a == QChar(0x201C) && b == QChar(0x201D));
        if (quoted) s = s.mid(1, s.size() - 2).trimmed();
    }
    return s;
}

QString stringOf(const QJsonValue& v) {
    return v.isString() ? v.toString() : v.toVariant().toString();
}

QString pick(const QJsonObject& o, const QStringList& keys) {
    for (const QString& k : keys) {
        if (o.contains(k)) return stringOf(o.value(k)).trimmed();
    }
    return {};
}

bool isHeaderRow(const QString& left, const QString& right) {
    static const QSet<QString> names = { "source", "src", "term", "heard", "wrong", "from", "original",
                                         "target", "translation", "correct", "to", "译文", "原文", "听成",
                                         "写成", "源词", "术语", "翻译" };
    return names.contains(left.trimmed().toLower()) && names.contains(right.trimmed().toLower());
}

// 引号包住的 CSV 行：字段内可含逗号，"" 表示一个引号
QStringList csvFields(const QString& line) {
    QStringList fields;
    QString cur;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (quoted) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; }
            else if (c == '"') quoted = false;
            else cur += c;
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            fields << cur;
            cur.clear();
        } else {
            cur += c;
        }
    }
    fields << cur;
    return fields;
}

// 按优先级找分隔符：明确的箭头和等号最先，逗号最后（逗号在听成一栏里还用来分隔多个说法）
bool splitPair(const QString& line, QString* left, QString* right) {
    static const QStringList strong = { "=>", "->", QStringLiteral("→"), QStringLiteral("⇒"), "\t", "=", "|" };
    for (const QString& sep : strong) {
        const int i = line.indexOf(sep);
        if (i < 0) continue;
        *left = line.left(i);
        *right = line.mid(i + sep.size());
        return true;
    }
    int colon = line.indexOf(QStringLiteral("："));
    int width = 1;
    if (colon < 0) {
        colon = line.indexOf(QStringLiteral(": "));
        width = 2;
    }
    if (colon >= 0) {
        *left = line.left(colon);
        *right = line.mid(colon + width);
        return true;
    }
    if (line.trimmed().startsWith('"')) {
        const QStringList f = csvFields(line.trimmed());
        if (f.size() >= 2) {
            *left = f[0];
            *right = f[1];
            return true;
        }
    }
    const int commas = line.count(',') + line.count(QStringLiteral("，"));
    if (commas == 1) {
        const int i = qMax(line.indexOf(','), line.indexOf(QStringLiteral("，")));
        *left = line.left(i);
        *right = line.mid(i + 1);
        return true;
    }
    return false;
}

// 软件自己导出的 TSV：启用标记 / 听成 / 写成 / 别名 / 模式 / 标签 / 备注
bool parseNativeRow(const QString& line, Entry* e) {
    const QStringList c = line.split('\t');
    if (c.size() < 3 || (c[0] != QLatin1String("0") && c[0] != QLatin1String("1"))) return false;
    e->enabled = c[0] == QLatin1String("1");
    e->heard = c[1];
    if (!c.value(3).trimmed().isEmpty()) e->heard += QStringLiteral(", ") + c[3];
    e->correct = c[2];
    e->tags = c.value(5);
    e->notes = c.value(6);
    return true;
}

void addEntryFromPair(const QString& rawLeft, const QString& rawRight, int lineNo, const QString& source,
                      bool requireHeard, Parsed* out) {
    const QString left = stripQuotes(rawLeft);
    const QString right = stripQuotes(rawRight);
    if (right.isEmpty()) {
        out->issues.append({ lineNo, source, QCoreApplication::translate("GlossaryImport", "Missing the written / translated side") });
        return;
    }
    if (left.isEmpty() && requireHeard) {
        out->issues.append({ lineNo, source, QCoreApplication::translate("GlossaryImport", "Missing the source term") });
        return;
    }
    Entry e;
    e.heard = left;
    e.correct = right;
    out->entries.append(e);
}

void parseOneLine(const QString& line, int lineNo, bool requireHeard, bool* nativeRows, Parsed* out) {
    Entry native;
    if (parseNativeRow(line, &native)) {
        *nativeRows = true;
        out->entries.append(native);
        return;
    }

    QString left;
    QString right;
    if (splitPair(line, &left, &right)) {
        if (out->entries.isEmpty() && out->issues.isEmpty() && isHeaderRow(left, right)) return;
        addEntryFromPair(left, right, lineNo, line, requireHeard, out);
        return;
    }
    if (requireHeard) {
        out->issues.append({ lineNo, line, QCoreApplication::translate("GlossaryImport", "No separator found (use =>, ->, tab, = or comma)") });
        return;
    }
    // 识别词典里单独一个词：只让识别器认识它
    if (line.count(',') + line.count(QStringLiteral("，")) > 1) {
        out->issues.append({ lineNo, line, QCoreApplication::translate("GlossaryImport", "Cannot tell where \"heard as\" ends; use => between the two sides") });
        return;
    }
    Entry e;
    e.correct = stripQuotes(line);
    out->entries.append(e);
}

void entriesFromJsonArray(const QJsonArray& array, Parsed* out, bool requireHeard) {
    for (const QJsonValue& v : array) {
        Entry e;
        if (v.isObject()) {
            const QJsonObject o = v.toObject();
            e.heard = pick(o, kHeardKeys);
            e.correct = pick(o, kCorrectKeys);
            e.notes = pick(o, kNoteKeys);
        } else if (v.isArray()) {
            const QJsonArray pair = v.toArray();
            e.heard = stringOf(pair.at(0)).trimmed();
            e.correct = stringOf(pair.at(1)).trimmed();
        } else if (v.isString()) {
            Parsed single;
            bool native = false;
            parseOneLine(v.toString(), 0, requireHeard, &native, &single);
            out->entries.append(single.entries);
            out->issues.append(single.issues);
            continue;
        } else {
            continue;
        }
        if (e.correct.isEmpty() || (requireHeard && e.heard.isEmpty())) {
            out->issues.append({ 0, QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact)),
                                 QCoreApplication::translate("GlossaryImport", "Entry is missing a source or a translation") });
            continue;
        }
        out->entries.append(e);
    }
}

void entriesFromJsonMap(const QJsonObject& map, Parsed* out, bool requireHeard) {
    for (auto it = map.begin(); it != map.end(); ++it) {
        const QString heard = it.key().trimmed();
        const QString correct = stringOf(it.value()).trimmed();
        if (correct.isEmpty() || (requireHeard && heard.isEmpty())) {
            out->issues.append({ 0, it.key(), QCoreApplication::translate("GlossaryImport", "Entry is missing a source or a translation") });
            continue;
        }
        Entry e;
        e.heard = heard;
        e.correct = correct;
        out->entries.append(e);
    }
}

bool allStrings(const QJsonObject& o) {
    if (o.isEmpty()) return false;
    for (auto it = o.begin(); it != o.end(); ++it) {
        if (!it.value().isString()) return false;
    }
    return true;
}

bool parseJson(const QJsonDocument& doc, bool requireHeard, Parsed* out) {
    if (doc.isArray()) {
        out->format = QStringLiteral("json");
        entriesFromJsonArray(doc.array(), out, requireHeard);
        return true;
    }
    if (!doc.isObject()) return false;
    const QJsonObject root = doc.object();

    if (root.value(QStringLiteral("format")).toString() == QLatin1String("aurapro-glossary")) {
        out->format = QStringLiteral("aurapro-glossary");
        const QJsonObject meta = root.value(QStringLiteral("glossary")).toObject();
        out->name = meta.value(QStringLiteral("name")).toString();
        out->version = meta.value(QStringLiteral("version")).toString();
        out->sourceLang = meta.value(QStringLiteral("source_lang")).toString();
        out->glossaryLang = meta.value(QStringLiteral("glossary_lang")).toString();
        if (out->glossaryLang.isEmpty()) out->glossaryLang = meta.value(QStringLiteral("target_lang")).toString();
    } else {
        out->format = QStringLiteral("json");
    }

    if (root.contains(QStringLiteral("entries"))) {
        const QJsonValue entries = root.value(QStringLiteral("entries"));
        if (entries.isArray()) entriesFromJsonArray(entries.toArray(), out, requireHeard);
        else entriesFromJsonMap(entries.toObject(), out, requireHeard);
        return true;
    }
    if (allStrings(root)) {
        entriesFromJsonMap(root, out, requireHeard);
        return true;
    }
    return false;
}

QString decode(const QByteArray& bytes) {
    QStringDecoder utf8(QStringConverter::Utf8);
    const QString text = utf8(bytes);
    if (!utf8.hasError()) return text;
    // 不是 UTF-8（常见于 Windows 下用记事本存的 GBK / ANSI 文件）
    QStringDecoder local(QStringConverter::System);
    return local(bytes);
}

}

Parsed parseText(const QString& rawText, bool requireHeard) {
    Parsed out;
    QString text = rawText;
    if (text.startsWith(QChar(0xFEFF))) text.remove(0, 1);

    const QString trimmed = text.trimmed();
    if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
        const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8());
        if (!doc.isNull() && parseJson(doc, requireHeard, &out)) return out;
        out = Parsed();
    }

    out.format = QStringLiteral("lines");
    bool nativeRows = false;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("\r\n|\n|\r")));
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        const QString probe = line.trimmed();
        if (probe.isEmpty() || probe.startsWith('#') || probe.startsWith(QStringLiteral("//"))) continue;
        const int lineNo = i + 1;

        // a=>b;c=>d：一行里连写多条规则（词库里 replaceRules 的写法）
        if (line.count(QStringLiteral("=>")) > 1 && (line.contains(';') || line.contains(QStringLiteral("；")))) {
            QString normalized = line;
            normalized.replace(QStringLiteral("；"), QStringLiteral(";"));
            for (const QString& part : normalized.split(';', Qt::SkipEmptyParts)) {
                if (!part.trimmed().isEmpty()) parseOneLine(part, lineNo, requireHeard, &nativeRows, &out);
            }
            continue;
        }
        parseOneLine(line, lineNo, requireHeard, &nativeRows, &out);
    }
    if (nativeRows) out.format = QStringLiteral("tsv");
    return out;
}

QString readTextFile(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QCoreApplication::translate("GlossaryImport", "Cannot open file");
        return {};
    }
    return decode(file.readAll());
}

Parsed parseFile(const QString& path, bool requireHeard, QString* error) {
    const QString text = readTextFile(path, error);
    if (error && !error->isEmpty()) return {};
    return parseText(text, requireHeard);
}

bool looksSwapped(const QList<Entry>& entries, const QString& langA, const QString& langB) {
    if (langA.isEmpty() || langB.isEmpty() || entries.isEmpty()) return false;
    int swapped = 0;
    int judged = 0;
    for (const Entry& e : entries) {
        if (e.heard.isEmpty()) continue;
        ++judged;
        if (EntryCheck::translation(e.heard, e.correct, langA, langB).verdict == EntryCheck::Verdict::SwapSuggested) ++swapped;
    }
    return judged > 0 && swapped * 2 > judged;
}

Summary mergeInto(TermsLibraryManager& library, const QList<Entry>& entries, bool requireHeard,
                  const QString& langA, const QString& langB, bool swap) {
    Summary summary;
    QVector<TermItem> items;
    QSet<QString> seenHeard;
    QSet<QString> seenHotword;

    for (const Entry& e : entries) {
        QString heard = e.heard;
        QString correct = e.correct;
        if (swap) std::swap(heard, correct);
        correct = correct.trimmed();
        const QStringList words = TermsLibraryManager::splitWords(heard);
        if (correct.isEmpty() || (requireHeard && words.isEmpty())) {
            ++summary.invalid;
            continue;
        }

        bool duplicate = false;
        if (words.isEmpty()) {
            duplicate = seenHotword.contains(correct);
            if (!duplicate) {
                for (const TermItem& existing : library.items()) {
                    if (existing.correct == correct && existing.heard().isEmpty()) { duplicate = true; break; }
                }
            }
        } else {
            for (const QString& w : words) {
                if (seenHeard.contains(w.toLower())) { duplicate = true; break; }
            }
            duplicate = duplicate || !library.duplicateHeard(words).isEmpty();
        }
        if (duplicate) {
            ++summary.duplicates;
            continue;
        }

        QString tag = e.tags;
        if (!langA.isEmpty()) {
            const EntryCheck::Result check = requireHeard ? EntryCheck::translation(heard, correct, langA, langB)
                                                          : EntryCheck::recognition(heard, correct, langA);
            if (check.verdict != EntryCheck::Verdict::Ok) ++summary.mismatched;
            else if (tag.isEmpty()) tag = check.tag;
        }

        TermItem item;
        item.setHeard(words);
        item.correct = correct;
        item.enabled = e.enabled;
        item.tags = tag;
        item.notes = e.notes;
        items.append(item);
        if (words.isEmpty()) seenHotword.insert(correct);
        for (const QString& w : words) seenHeard.insert(w.toLower());
    }

    if (!items.isEmpty()) library.addItems(items);
    summary.added = items.size();
    return summary;
}

}
