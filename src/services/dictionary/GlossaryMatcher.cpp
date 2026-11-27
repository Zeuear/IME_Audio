#include "GlossaryMatcher.h"

#include <algorithm>

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QVector>

#include <rapidfuzz/fuzz.hpp>

namespace {

const QString kLeft = QStringLiteral("（(【[「『\"'");
const QString kRight = QStringLiteral("）)】]」』\"'");

const QRegularExpression& bracketPattern() {
    static const QRegularExpression re(QStringLiteral("([（(【\\[「『\"'])([^）)】\\]」』\"']*)([）)】\\]」』\"'])"));
    return re;
}

const QRegularExpression& separators() {
    static const QRegularExpression re(QStringLiteral("[/／、,|｜]"));
    return re;
}

bool matchedPair(QChar left, QChar right) {
    const int i = kLeft.indexOf(left);
    return i >= 0 && kRight[i] == right;
}

QString normalizeBrackets(QString text) {
    text.replace(QLatin1Char('('), QStringLiteral("（"));
    text.replace(QLatin1Char(')'), QStringLiteral("）"));
    text.replace(QLatin1Char('['), QStringLiteral("【"));
    text.replace(QLatin1Char(']'), QStringLiteral("】"));
    return text;
}

QString normalizeTerm(const QString& text) {
    return normalizeBrackets(text.normalized(QString::NormalizationForm_KC)).simplified();
}

QString replaceBrackets(const QString& text, bool keepInner) {
    QString out;
    int last = 0;
    auto it = bracketPattern().globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        out += text.mid(last, m.capturedStart() - last);
        if (matchedPair(m.captured(1)[0], m.captured(3)[0])) {
            if (keepInner) out += m.captured(2);
        } else {
            out += m.captured(0);
        }
        last = m.capturedEnd();
    }
    out += text.mid(last);
    return out.simplified();
}

QStringList splitAliases(const QString& text) {
    QStringList result;
    QSet<QString> seen;
    for (const QString& part : text.split(separators())) {
        const QString p = normalizeTerm(part.trimmed());
        if (p.isEmpty() || seen.contains(p)) continue;
        seen.insert(p);
        result << p;
    }
    return result;
}

bool hasBracketAliases(const QString& text) {
    const auto m = bracketPattern().match(text);
    if (!m.hasMatch() || !matchedPair(m.captured(1)[0], m.captured(3)[0])) return false;
    return m.captured(2).contains(separators());
}

QStringList expandBracketAliases(const QString& text) {
    auto it = bracketPattern().globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        if (!matchedPair(m.captured(1)[0], m.captured(3)[0])) continue;
        const QStringList inner = m.captured(2).split(separators());
        if (inner.size() <= 1) continue;
        const QString prefix = text.left(m.capturedStart());
        const QString suffix = text.mid(m.capturedEnd());
        QStringList result;
        for (const QString& alias : inner) {
            const QString a = alias.trimmed();
            if (a.isEmpty()) continue;
            result << expandBracketAliases(prefix + a + suffix);
        }
        return result;
    }
    return { text };
}

struct Side {
    QStringList aliases;
    bool bracket = false;
};

Side parseSide(const QString& raw) {
    if (hasBracketAliases(raw)) return { expandBracketAliases(raw), true };
    return { splitAliases(raw), false };
}

QList<QPair<QString, QStringList>> pairKeysValues(const Side& keys, const Side& values) {
    QList<QPair<QString, QStringList>> pairs;
    if (!keys.bracket && !values.bracket && keys.aliases.size() == values.aliases.size()) {
        for (int i = 0; i < keys.aliases.size(); ++i) pairs.append({ keys.aliases[i], { values.aliases[i] } });
        return pairs;
    }
    for (const QString& k : keys.aliases) pairs.append({ k, values.aliases });
    return pairs;
}

QStringList bracketVariants(const QString& term) {
    QStringList variants{ term };
    const QString markers = replaceBrackets(term, true);
    if (markers != term) variants << markers;
    const QString all = replaceBrackets(term, false);
    if (all != term && !variants.contains(all)) variants << all;
    return variants;
}

bool isLatinTerm(const QString& term) {
    bool hasLetter = false;
    for (const QChar ch : term) {
        if (ch.unicode() >= 0x250) return false;
        if (ch.isLetter()) hasLetter = true;
    }
    return hasLetter;
}

QString stripSpaces(QString text) {
    text.remove(QRegularExpression(QStringLiteral("\\s+")));
    return text;
}

struct Candidate {
    QString key;
    QStringList targets;
    bool latin = false;
};

QList<Candidate> buildCandidates(const GlossaryMatcher::Pairs& entries) {
    QList<Candidate> list;
    QHash<QString, int> index;
    for (const auto& entry : entries) {
        if (entry.first.isEmpty() || entry.second.isEmpty()) continue;
        const auto pairs = pairKeysValues(parseSide(entry.first), parseSide(entry.second));
        for (const auto& pair : pairs) {
            for (const QString& variant : bracketVariants(pair.first)) {
                const bool latin = isLatinTerm(variant);
                const QString key = latin ? variant.simplified() : stripSpaces(variant);
                if (key.isEmpty()) continue;
                auto found = index.constFind(key);
                if (found == index.constEnd()) {
                    index.insert(key, int(list.size()));
                    list.append({ key, {}, latin });
                    found = index.constFind(key);
                }
                for (const QString& v : pair.second) {
                    if (!v.isEmpty() && !list[*found].targets.contains(v)) list[*found].targets << v;
                }
            }
        }
    }
    std::stable_sort(list.begin(), list.end(), [](const Candidate& a, const Candidate& b) { return a.key.size() > b.key.size(); });
    return list;
}

std::u16string_view view(const QString& s) {
    return std::u16string_view(reinterpret_cast<const char16_t*>(s.utf16()), size_t(s.size()));
}

using Span = QPair<int, int>;

struct Hit {
    const Candidate* candidate = nullptr;
    bool exact = false;
    double score = 0;
    QList<Span> spans;
};

bool wordChar(QChar ch) {
    return ch.isLetterOrNumber();
}

bool onBoundary(const QString& text, int start, int end) {
    return (start == 0 || !wordChar(text[start - 1])) && (end == text.size() || !wordChar(text[end]));
}

QList<Span> findAll(const QString& text, const QString& term, bool wholeWord) {
    QList<Span> spans;
    int from = 0;
    while (true) {
        const int idx = text.indexOf(term, from, wholeWord ? Qt::CaseInsensitive : Qt::CaseSensitive);
        if (idx < 0) break;
        if (!wholeWord || onBoundary(text, idx, idx + term.size())) spans.append({ idx, idx + int(term.size()) });
        from = idx + 1;
    }
    return spans;
}

}

namespace GlossaryMatcher {

Pairs match(const Pairs& entries, const QString& text, int maxTerms, double fuzzyThreshold) {
    const QList<Candidate> candidates = buildCandidates(entries);
    const QString clean = stripSpaces(text);
    const QString spaced = text.simplified();
    const QString spacedLower = spaced.toLower();
    const QSet<QChar> cleanChars(clean.begin(), clean.end());

    QList<Hit> hits;
    QSet<QString> seen;
    for (const Candidate& c : candidates) {
        const QString folded = c.key.toCaseFolded();
        if (seen.contains(folded)) continue;

        const QString& haystack = c.latin ? spaced : clean;
        QList<Span> exact = findAll(haystack, c.key, c.latin);
        if (!exact.isEmpty()) {
            hits.append({ &c, true, 100.0, exact });
            seen.insert(folded);
        } else if (c.latin) {
            if (c.key.size() >= 4 && c.key.size() <= spaced.size()) {
                const QString needle = c.key.toLower();
                const auto alignment = rapidfuzz::fuzz::partial_ratio_alignment(view(needle), view(spacedLower), fuzzyThreshold);
                const int s = int(alignment.dest_start);
                const int e = int(alignment.dest_end);
                if (alignment.score >= fuzzyThreshold && onBoundary(spaced, s, e)) {
                    hits.append({ &c, false, alignment.score, { { s, e } } });
                    seen.insert(folded);
                }
            }
        } else {
            const QSet<QChar> termChars(c.key.begin(), c.key.end());
            int common = 0;
            for (const QChar ch : termChars) common += cleanChars.contains(ch) ? 1 : 0;
            if (double(common) / std::max<int>(int(termChars.size()), 1) < 0.6) continue;
            if (c.key.size() > clean.size()) continue;
            const auto alignment = rapidfuzz::fuzz::partial_ratio_alignment(view(c.key), view(clean), fuzzyThreshold);
            if (alignment.score >= fuzzyThreshold) {
                hits.append({ &c, false, alignment.score, { { int(alignment.dest_start), int(alignment.dest_end) } } });
                seen.insert(folded);
            }
        }
        if (int(hits.size()) >= maxTerms) break;
    }

    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        if (a.exact != b.exact) return a.exact;
        if (a.candidate->key.size() != b.candidate->key.size()) return a.candidate->key.size() > b.candidate->key.size();
        return a.score > b.score;
    });

    QVector<char> coveredClean(int(clean.size()), 0);
    QVector<char> coveredSpaced(int(spaced.size()), 0);
    Pairs result;
    for (const Hit& hit : hits) {
        QVector<char>& covered = hit.candidate->latin ? coveredSpaced : coveredClean;
        bool uncovered = false;
        for (const Span& span : hit.spans) {
            if (span.first < 0 || span.second > covered.size()) continue;
            for (int i = span.first; i < span.second; ++i) uncovered |= !covered[i];
        }
        if (!uncovered) continue;
        for (const Span& span : hit.spans) {
            if (span.first < 0 || span.second > covered.size() || span.first >= span.second) continue;
            for (int i = span.first; i < span.second; ++i) covered[i] = 1;
        }
        if (hit.candidate->targets.isEmpty()) continue;
        result.append({ hit.candidate->key, hit.candidate->targets.join(QLatin1Char('/')) });
    }
    return result;
}

}
