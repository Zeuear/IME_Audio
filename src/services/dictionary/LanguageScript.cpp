#include "LanguageScript.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QVector>

namespace {

enum class Script {
    None, Han, Kana, Hangul, Latin, Cyrillic, Greek, Armenian, Hebrew, Arabic,
    Devanagari, Bengali, Tamil, Thai, Lao, Tibetan, Myanmar, Georgian, Khmer
};

struct Range {
    char32_t low;
    char32_t high;
    Script script;
};

const QVector<Range>& ranges() {
    static const QVector<Range> table = {
        { 0x3400, 0x4DBF, Script::Han }, { 0x4E00, 0x9FFF, Script::Han }, { 0xF900, 0xFAFF, Script::Han },
        { 0x3040, 0x30FF, Script::Kana }, { 0x31F0, 0x31FF, Script::Kana },
        { 0x1100, 0x11FF, Script::Hangul }, { 0x3130, 0x318F, Script::Hangul }, { 0xAC00, 0xD7AF, Script::Hangul },
        { 0x41, 0x5A, Script::Latin }, { 0x61, 0x7A, Script::Latin }, { 0xC0, 0xD6, Script::Latin },
        { 0xD8, 0xF6, Script::Latin }, { 0xF8, 0x24F, Script::Latin }, { 0x1E00, 0x1EFF, Script::Latin },
        { 0x400, 0x52F, Script::Cyrillic },
        { 0x370, 0x3FF, Script::Greek },
        { 0x531, 0x58F, Script::Armenian },
        { 0x5D0, 0x5FF, Script::Hebrew },
        { 0x600, 0x6FF, Script::Arabic }, { 0x750, 0x77F, Script::Arabic }, { 0x8A0, 0x8FF, Script::Arabic },
        { 0x900, 0x97F, Script::Devanagari },
        { 0x980, 0x9FF, Script::Bengali },
        { 0xB80, 0xBFF, Script::Tamil },
        { 0xE00, 0xE7F, Script::Thai },
        { 0xE80, 0xEFF, Script::Lao },
        { 0xF00, 0xFFF, Script::Tibetan },
        { 0x1000, 0x109F, Script::Myanmar },
        { 0x10A0, 0x10FF, Script::Georgian }, { 0x1C90, 0x1CBF, Script::Georgian },
        { 0x1780, 0x17FF, Script::Khmer },
    };
    return table;
}

Script scriptOf(char32_t ch) {
    for (const Range& r : ranges()) {
        if (ch >= r.low && ch <= r.high) return r.script;
    }
    return Script::None;
}

int weightOf(Script script) {
    return (script == Script::Han || script == Script::Kana || script == Script::Hangul) ? 3 : 1;
}

const QHash<QString, QSet<Script>>& languageScripts() {
    static const QHash<QString, QSet<Script>> table = [] {
        QHash<QString, QSet<Script>> t;
        t.insert(QStringLiteral("zh"), { Script::Han });
        t.insert(QStringLiteral("ja"), { Script::Han, Script::Kana });
        t.insert(QStringLiteral("ko"), { Script::Hangul });
        t.insert(QStringLiteral("sr"), { Script::Cyrillic, Script::Latin });
        for (const char* c : { "ru", "uk", "bg", "mk", "be", "kk", "ky", "mn", "tg" }) t.insert(QLatin1String(c), { Script::Cyrillic });
        for (const char* c : { "ar", "fa", "ur", "ug", "ps" }) t.insert(QLatin1String(c), { Script::Arabic });
        for (const char* c : { "hi", "ne", "mr", "sa" }) t.insert(QLatin1String(c), { Script::Devanagari });
        t.insert(QStringLiteral("el"), { Script::Greek });
        t.insert(QStringLiteral("hy"), { Script::Armenian });
        t.insert(QStringLiteral("he"), { Script::Hebrew });
        t.insert(QStringLiteral("bn"), { Script::Bengali });
        t.insert(QStringLiteral("ta"), { Script::Tamil });
        t.insert(QStringLiteral("th"), { Script::Thai });
        t.insert(QStringLiteral("lo"), { Script::Lao });
        t.insert(QStringLiteral("bo"), { Script::Tibetan });
        t.insert(QStringLiteral("my"), { Script::Myanmar });
        t.insert(QStringLiteral("ka"), { Script::Georgian });
        t.insert(QStringLiteral("km"), { Script::Khmer });
        for (const char* c : { "en", "es", "fr", "de", "it", "pt", "nl", "pl", "ro", "sk", "cs", "sl", "hr", "bs", "hu",
                               "id", "ms", "tl", "fil", "sw", "vi", "hmn", "tr", "sv", "da", "no", "nb", "fi", "et", "lv",
                               "lt", "ca", "uz", "az", "af", "is", "sq", "eu", "gl", "ga", "cy", "mt" }) {
            t.insert(QLatin1String(c), { Script::Latin });
        }
        return t;
    }();
    return table;
}

const QHash<QString, QString>& latinCues() {
    static const QHash<QString, QString> table = {
        { QStringLiteral("es"), QStringLiteral("ñ¿¡áéíóúü") },
        { QStringLiteral("pt"), QStringLiteral("ãõçáéíóúâêôà") },
        { QStringLiteral("fr"), QStringLiteral("àâçèéêëîïôùûüÿœæ") },
        { QStringLiteral("de"), QStringLiteral("äöüß") },
        { QStringLiteral("it"), QStringLiteral("àèéìòù") },
        { QStringLiteral("pl"), QStringLiteral("ąćęłńóśźż") },
        { QStringLiteral("tr"), QStringLiteral("çğıöşü") },
        { QStringLiteral("ro"), QStringLiteral("ăâîșțşţ") },
    };
    return table;
}

}

namespace LanguageScript {

QString family(const QString& code) {
    QString base = code.trimmed().toLower();
    const int cut = base.indexOf(QRegularExpression(QStringLiteral("[-_]")));
    if (cut >= 0) base = base.left(cut);
    static const QSet<QString> chinese = { QStringLiteral("yue"), QStringLiteral("cmn"), QStringLiteral("wuu"),
                                           QStringLiteral("hak"), QStringLiteral("nan"), QStringLiteral("gan"),
                                           QStringLiteral("hsn"), QStringLiteral("cjy"), QStringLiteral("lzh") };
    return chinese.contains(base) ? QStringLiteral("zh") : base;
}

bool contradicts(const QString& text, const QString& languageFamily) {
    const auto it = languageScripts().constFind(family(languageFamily));
    if (it == languageScripts().constEnd()) return false;
    const QSet<Script>& own = it.value();

    int inside = 0;
    int outside = 0;
    for (const auto ch : text.toUcs4()) {
        const Script script = scriptOf(ch);
        if (script == Script::None) continue;
        if (own.contains(script)) inside += weightOf(script);
        else if (script != Script::Latin) outside += weightOf(script);
    }
    return outside > 0 && outside >= inside;
}

Side sideOf(const QString& text, const QString& familyA, const QString& familyB) {
    const QString a = family(familyA);
    const QString b = family(familyB);
    const auto ia = languageScripts().constFind(a);
    const auto ib = languageScripts().constFind(b);
    if (ia == languageScripts().constEnd() || ib == languageScripts().constEnd()) return Side::Unknown;

    const QSet<Script> onlyA = QSet<Script>(ia.value()).subtract(ib.value());
    const QSet<Script> onlyB = QSet<Script>(ib.value()).subtract(ia.value());
    const auto chars = text.toUcs4();

    int scoreA = 0;
    int scoreB = 0;
    for (const auto ch : chars) {
        const Script script = scriptOf(ch);
        if (onlyA.contains(script)) scoreA += weightOf(script);
        else if (onlyB.contains(script)) scoreB += weightOf(script);
    }
    if (scoreA != scoreB) return scoreA > scoreB ? Side::A : Side::B;

    const QString cuesA = latinCues().value(a);
    const QString cuesB = latinCues().value(b);
    int cueA = 0;
    int cueB = 0;
    for (const QChar ch : text.toLower()) {
        const bool inA = cuesA.contains(ch);
        const bool inB = cuesB.contains(ch);
        if (inA && !inB) ++cueA;
        else if (inB && !inA) ++cueB;
    }
    if (cueA == cueB) return Side::Unknown;
    return cueA > cueB ? Side::A : Side::B;
}

}
