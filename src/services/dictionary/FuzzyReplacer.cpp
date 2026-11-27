#include "FuzzyReplacer.h"

#include <algorithm>

#include <rapidfuzz/fuzz.hpp>

namespace {

constexpr char32_t kPinyinBase = 0x100000;

bool isAsciiWordChar(QChar ch) {
    return ch.unicode() < 0x80 && ch.isLetterOrNumber();
}

struct Word {
    int start;
    int end;
};

QList<Word> asciiWords(const QString& text) {
    QList<Word> words;
    int i = 0;
    while (i < text.size()) {
        if (!isAsciiWordChar(text[i])) { ++i; continue; }
        const int start = i;
        while (i < text.size() && isAsciiWordChar(text[i])) ++i;
        words.append({ start, i });
    }
    return words;
}

bool onlySpaces(const QString& text, int from, int to) {
    for (int i = from; i < to; ++i) {
        if (!text[i].isSpace()) return false;
    }
    return true;
}

std::u16string_view view(const QString& s) {
    return std::u16string_view(reinterpret_cast<const char16_t*>(s.utf16()), size_t(s.size()));
}

}

FuzzyReplacer::FuzzyReplacer(const PinyinTable* table)
    : m_table(table ? table : &PinyinTable::builtin()) {
}

std::u32string FuzzyReplacer::pinyinOf(const QString& text, QList<int>* positions) const {
    std::u32string out;
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text[i];
        const int sid = m_table->syllableId(ch);
        if (sid >= 0) {
            out.push_back(kPinyinBase + char32_t(m_table->normId(sid)));
        } else if (ch.isLetterOrNumber()) {
            out.push_back(char32_t(ch.toLower().unicode()));
        } else {
            continue;
        }
        if (positions) positions->append(i);
    }
    return out;
}

FuzzyReplacer::Form FuzzyReplacer::compile(const QString& text) const {
    Form form;
    bool hasHan = false;
    for (const QChar ch : text) {
        if (m_table->syllableId(ch) >= 0) { hasHan = true; break; }
    }
    if (hasHan) {
        form.pinyin = pinyinOf(text);
        return form;
    }
    const QString trimmed = text.trimmed();
    int words = 0;
    for (const Word& w : asciiWords(trimmed)) { Q_UNUSED(w); ++words; }
    int letters = 0;
    for (const QChar ch : trimmed) letters += isAsciiWordChar(ch) ? 1 : 0;
    if (letters < kMinLatinLength || words == 0) return form;
    QString joined;
    for (const Word& w : asciiWords(trimmed)) {
        if (!joined.isEmpty()) joined += QLatin1Char(' ');
        joined += trimmed.mid(w.start, w.end - w.start).toLower();
    }
    form.latin = joined;
    form.latinWords = words;
    return form;
}

void FuzzyReplacer::setEntries(const QList<HotwordSelector::Entry>& entries) {
    m_entries.clear();
    for (const auto& e : entries) {
        if (e.text.trimmed().isEmpty()) continue;
        Entry entry;
        entry.correct = e.text;
        QStringList sources = e.heard;
        sources.prepend(e.text);
        for (const QString& s : sources) {
            Form f = compile(s);
            const bool usable = int(f.pinyin.size()) >= kMinPinyinUnits || !f.latin.isEmpty();
            if (usable) entry.forms.append(std::move(f));
        }
        if (!entry.forms.isEmpty()) m_entries.append(std::move(entry));
    }
}

void FuzzyReplacer::matchPinyin(const Entry& entry, const std::u32string& haystack, const QList<int>& positions,
                                const QString& text, QList<Match>& out) const {
    if (text.contains(entry.correct)) return;
    Match best;
    for (const Form& form : entry.forms) {
        if (form.pinyin.empty() || haystack.size() < form.pinyin.size()) continue;
        const auto alignment = rapidfuzz::fuzz::partial_ratio_alignment(form.pinyin, haystack, kMinPinyinScore);
        if (alignment.score < kMinPinyinScore || alignment.score <= best.score) continue;
        const size_t len = alignment.dest_end - alignment.dest_start;
        if (len < size_t(kMinPinyinUnits)) continue;
        best.start = positions[int(alignment.dest_start)];
        best.end = positions[int(alignment.dest_end) - 1] + 1;
        best.score = alignment.score;
    }
    if (best.score <= 0) return;
    best.replacement = entry.correct;
    out.append(best);
}

void FuzzyReplacer::matchLatin(const Entry& entry, const QString& text, QList<Match>& out) const {
    const QString lowerText = text.toLower();
    const QString lowerCorrect = entry.correct.toLower();
    const QList<Word> words = asciiWords(text);
    Match best;
    for (const Form& form : entry.forms) {
        if (form.latin.isEmpty()) continue;
        for (int span = std::max(1, form.latinWords - 1); span <= form.latinWords + 1; ++span) {
            for (int i = 0; i + span <= words.size(); ++i) {
                bool contiguous = true;
                QString joined;
                for (int k = 0; k < span; ++k) {
                    if (k > 0 && !onlySpaces(text, words[i + k - 1].end, words[i + k].start)) { contiguous = false; break; }
                    if (k > 0) joined += QLatin1Char(' ');
                    joined += lowerText.mid(words[i + k].start, words[i + k].end - words[i + k].start);
                }
                if (!contiguous) continue;
                const double score = rapidfuzz::fuzz::ratio(view(form.latin), view(joined), kMinLatinScore);
                if (score < kMinLatinScore || score <= best.score) continue;
                best.start = words[i].start;
                best.end = words[i + span - 1].end;
                best.score = score;
            }
        }
    }
    if (best.score <= 0) return;
    const QString matched = text.mid(best.start, best.end - best.start);
    if (matched == entry.correct) return;
    if (matched.toLower() == lowerCorrect && text.contains(entry.correct)) return;
    best.replacement = entry.correct;
    out.append(best);
}

QString FuzzyReplacer::apply(const QString& text) const {
    if (m_entries.isEmpty() || text.trimmed().isEmpty()) return text;

    QList<int> positions;
    const std::u32string haystack = pinyinOf(text, &positions);

    QList<Match> matches;
    for (const Entry& entry : m_entries) {
        matchPinyin(entry, haystack, positions, text, matches);
        matchLatin(entry, text, matches);
    }
    if (matches.isEmpty()) return text;

    std::stable_sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) { return a.score > b.score; });
    QList<Match> accepted;
    for (const Match& m : matches) {
        const bool overlaps = std::any_of(accepted.begin(), accepted.end(),
            [&](const Match& a) { return m.start < a.end && a.start < m.end; });
        if (!overlaps) accepted.append(m);
    }
    std::sort(accepted.begin(), accepted.end(), [](const Match& a, const Match& b) { return a.start > b.start; });

    QString result = text;
    for (const Match& m : accepted) result.replace(m.start, m.end - m.start, m.replacement);
    return result;
}
