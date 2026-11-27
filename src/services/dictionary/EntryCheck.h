#pragma once
#include <QString>

namespace EntryCheck {

enum class Verdict { Ok, Mismatch, SwapSuggested };

struct Result {
    Verdict verdict = Verdict::Ok;
    QString tag;
};

Result recognition(const QString& heard, const QString& correct, const QString& language);
Result translation(const QString& source, const QString& target, const QString& langA, const QString& langB);

}
