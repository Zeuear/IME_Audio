#pragma once
#include <QString>

namespace LanguageScript {

enum class Side { A, B, Unknown };

QString family(const QString& code);
bool contradicts(const QString& text, const QString& languageFamily);
Side sideOf(const QString& text, const QString& familyA, const QString& familyB);

}
