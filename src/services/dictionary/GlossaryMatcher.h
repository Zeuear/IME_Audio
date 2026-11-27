#pragma once
#include <QList>
#include <QPair>
#include <QString>

// @brief 翻译词库匹配：从句子里挑出用得上的「原词→译词」，移植自 AuraPro 的 GlossaryMatcher。
namespace GlossaryMatcher {

using Pairs = QList<QPair<QString, QString>>;

constexpr int kDefaultMaxTerms = 10;
constexpr double kDefaultFuzzyThreshold = 70.0;

Pairs match(const Pairs& entries, const QString& text, int maxTerms = kDefaultMaxTerms,
            double fuzzyThreshold = kDefaultFuzzyThreshold);

}
