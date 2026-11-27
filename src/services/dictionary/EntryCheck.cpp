#include "EntryCheck.h"

#include "LanguageScript.h"

namespace EntryCheck {

Result recognition(const QString& heard, const QString& correct, const QString& language) {
    Result result;
    if (language.isEmpty()) return result;
    if (LanguageScript::contradicts(heard, language) || LanguageScript::contradicts(correct, language)) {
        result.verdict = Verdict::Mismatch;
        return result;
    }
    result.tag = QStringLiteral("lang:%1").arg(LanguageScript::family(language));
    return result;
}

Result translation(const QString& source, const QString& target, const QString& langA, const QString& langB) {
    using LanguageScript::Side;
    Result result;
    const Side sourceSide = LanguageScript::sideOf(source, langA, langB);
    const Side targetSide = LanguageScript::sideOf(target, langA, langB);
    const bool swapEvidence = sourceSide == Side::B || targetSide == Side::A;
    const bool okEvidence = sourceSide == Side::A || targetSide == Side::B;
    if (swapEvidence && !okEvidence) {
        result.verdict = Verdict::SwapSuggested;
        return result;
    }
    result.tag = QStringLiteral("lang:%1>%2").arg(LanguageScript::family(langA), LanguageScript::family(langB));
    return result;
}

}
