#include "DictionaryLanguages.h"

#include <QCoreApplication>
#include <QSet>

#include "LanguageScript.h"
#include "sherpa/LanguageCatalog.h"
#include "translation/LanguageRouting.h"

namespace DictionaryLanguages {

QList<Choice> choices() {
    QList<Choice> out;
    QSet<QString> seen;
    for (const CloudTargetLanguage& lang : cloudTargetLanguages()) {
        const QString code = LanguageScript::family(QString::fromLatin1(lang.code));
        if (seen.contains(code)) continue;
        seen.insert(code);
        const QString key = LanguageCatalog::keyOfCode(code);
        out.append({ code, key.isEmpty() ? QCoreApplication::translate("MainWin", lang.label) : LanguageCatalog::plainName(key) });
    }
    return out;
}

QString label(const QString& code) {
    const QString family = LanguageScript::family(code);
    for (const Choice& c : choices()) {
        if (c.code == family) return c.label;
    }
    return code;
}

namespace {

// 「保加利亚语 / 保加利亚文」只差一个字，比较前去掉
QString stem(QString s) {
    if (s.size() > 1 && (s.endsWith(QStringLiteral("语")) || s.endsWith(QStringLiteral("文"))
                         || s.endsWith(QStringLiteral("語")) || s.endsWith(QStringLiteral("話")))) s.chop(1);
    return s;
}

}

QString codeFromName(const QString& name) {
    const QString key = name.trimmed().toLower();
    if (key.isEmpty()) return {};
    const QString s = stem(key);
    static const QStringList chinese = { QStringLiteral("中"), QStringLiteral("汉"), QStringLiteral("简体中"),
                                         QStringLiteral("繁体中"), QStringLiteral("普通话"), QStringLiteral("国"),
                                         QStringLiteral("chinese"), QStringLiteral("mandarin") };
    if (chinese.contains(s)) return LanguageScript::family(QStringLiteral("zh"));

    const QList<Choice> all = choices();
    const QString asCode = LanguageScript::family(key);
    for (const Choice& c : all) {
        if (c.code == asCode || c.label.toLower() == key || stem(c.label.toLower()) == s) return c.code;
    }
    for (const CloudTargetLanguage& lang : cloudTargetLanguages()) {
        if (stem(QString::fromUtf8(lang.zhName)) == s) return LanguageScript::family(QString::fromLatin1(lang.code));
    }
    return {};
}

QString subtitle(const DictionaryProfile& profile) {
    if (profile.kind == DictionaryKind::Translation) {
        return QCoreApplication::translate("DictionaryLanguages", "%1 ↔ %2").arg(label(profile.langA), label(profile.langB));
    }
    if (profile.langA.isEmpty()) return QCoreApplication::translate("DictionaryLanguages", "Any language");
    return label(profile.langA);
}

}
