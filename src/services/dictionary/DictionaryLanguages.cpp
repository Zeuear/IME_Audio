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

QString subtitle(const DictionaryProfile& profile) {
    if (profile.kind == DictionaryKind::Translation) {
        return QCoreApplication::translate("DictionaryLanguages", "%1 ↔ %2").arg(label(profile.langA), label(profile.langB));
    }
    if (profile.langA.isEmpty()) return QCoreApplication::translate("DictionaryLanguages", "Any language");
    return label(profile.langA);
}

}
