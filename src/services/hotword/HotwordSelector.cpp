#include "HotwordSelector.h"

#include <algorithm>
#include <cmath>

namespace {

// 英文词之间的差别按字符算：识别器把 Kubernetes 写成 Kubernetis 仍然算很像
double latinCost(const QString& a, const QString& b)
{
    if (a == b) return 0.0;
    const int n = int(a.size()), m = int(b.size());
    QVector<int> prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; ++j) prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        for (int j = 1; j <= m; ++j)
            cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] != b[j - 1]) });
        std::swap(prev, cur);
    }
    return double(prev[m]) / std::max(n, m);
}

} // namespace

HotwordSelector::HotwordSelector(const PinyinTable* table)
    : m_table(table ? table : &PinyinTable::builtin())
{
}

QVector<HotwordSelector::Unit> HotwordSelector::units(const QString& text) const
{
    QVector<Unit> out;
    QString run;
    auto flush = [&]() {
        if (run.isEmpty()) return;
        Unit u;
        u.latin = run;
        out.push_back(std::move(u));
        run.clear();
    };
    for (const QChar ch : text) {
        if (ch.unicode() < 0x80 && ch.isLetterOrNumber()) {
            run += ch.toLower();
            continue;
        }
        flush();
        const int sid = m_table->syllableId(ch);
        if (sid < 0) continue;
        Unit u;
        u.sid = sid;
        u.nid = m_table->normId(sid);
        u.altSid = m_table->alternateIds(ch);
        for (const int a : u.altSid) u.altNid.push_back(m_table->normId(a));
        out.push_back(std::move(u));
    }
    flush();
    return out;
}

double HotwordSelector::unitCost(const Unit& a, const Unit& b) const
{
    if (a.isLatin() != b.isLatin()) return 1.0;
    if (a.isLatin()) return latinCost(a.latin, b.latin);
    if (a.sid == b.sid) return 0.0;
    double best = a.nid == b.nid ? 0.3 : 1.0;
    if (a.altSid.isEmpty() && b.altSid.isEmpty()) return best;
    // 多音字：把主读音和其余读音都拿出来两两比，取最小
    const int an = int(a.altSid.size()), bn = int(b.altSid.size());
    for (int i = -1; i < an; ++i) {
        const int as = i < 0 ? a.sid : a.altSid[i];
        const int ana = i < 0 ? a.nid : a.altNid[i];
        for (int j = -1; j < bn; ++j) {
            const int bs = j < 0 ? b.sid : b.altSid[j];
            const int bna = j < 0 ? b.nid : b.altNid[j];
            if (as == bs) return 0.0;
            if (ana == bna) best = std::min(best, 0.3);
        }
    }
    return best;
}

double HotwordSelector::similarity(const QVector<Unit>& term, const QVector<Unit>& text) const
{
    const int n = int(term.size());
    if (n == 0 || text.isEmpty()) return 0.0;
    // 单个汉字的词条太容易凑巧同音，不参与
    if (n == 1 && !term[0].isLatin()) return 0.0;

    double best = 0.0;
    const int lens[3] = { std::max(1, n - 1), n, n + 1 };
    QVector<double> prev, cur;
    for (int li = 0; li < 3; ++li) {
        const int len = lens[li];
        if (li > 0 && len == lens[li - 1]) continue;   // n == 1 时 n - 1 被夹到 1，别重复算
        const int lastStart = std::max(0, int(text.size()) - len);
        for (int start = 0; start <= lastStart; ++start) {
            const int wl = std::min<int>(len, int(text.size()) - start);
            // 词条与窗口的编辑距离
            prev.fill(0.0, wl + 1);
            cur.fill(0.0, wl + 1);
            for (int j = 0; j <= wl; ++j) prev[j] = j;
            for (int i = 1; i <= n; ++i) {
                cur[0] = i;
                for (int j = 1; j <= wl; ++j)
                    cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1,
                                        prev[j - 1] + unitCost(term[i - 1], text[start + j - 1]) });
                std::swap(prev, cur);
            }
            best = std::max(best, 1.0 - prev[wl] / std::max(n, wl));
        }
    }
    return best;
}

bool HotwordSelector::mayMatch(const QVector<Unit>& form, const QSet<int>& textNids, bool textHasLatin, double minScore) const
{
    const int n = int(form.size());
    if (n == 0) return false;
    // 没对上的单元每个至少让编辑距离加 1，相似度 ≤ 1 - 没对上数 / (n + 1)；超过这个数就不可能达标
    const int allowedMiss = int(std::floor((1.0 - minScore) * (n + 1) + 1e-9));
    int miss = 0;
    for (const Unit& u : form) {
        bool hit = false;
        if (u.isLatin()) {
            hit = textHasLatin;
        } else {
            hit = textNids.contains(u.nid);
            for (int i = 0; !hit && i < u.altNid.size(); ++i) hit = textNids.contains(u.altNid[i]);
        }
        if (!hit && ++miss > allowedMiss) return false;
    }
    return true;
}

void HotwordSelector::setEntries(const QList<Entry>& entries)
{
    m_entries.clear();
    m_entries.reserve(entries.size());
    for (const Entry& e : entries) {
        if (e.text.trimmed().isEmpty()) continue;
        Compiled c;
        c.text = e.text;
        c.forms.push_back(units(e.text));
        for (const QString& h : e.heard)
            if (!h.trimmed().isEmpty()) c.forms.push_back(units(h));
        m_entries.push_back(std::move(c));
    }
}

QStringList HotwordSelector::select(const QString& text, int maxWords, double minScore) const
{
    if (maxWords <= 0 || m_entries.isEmpty()) return {};
    const QVector<Unit> heard = units(text);
    if (heard.isEmpty()) return {};

    QSet<int> textNids;
    bool textHasLatin = false;
    for (const Unit& u : heard) {
        if (u.isLatin()) { textHasLatin = true; continue; }
        textNids.insert(u.nid);
        for (const int a : u.altNid) textNids.insert(a);
    }

    struct Hit { double score; int order; };
    QVector<Hit> hits;
    for (int i = 0; i < m_entries.size(); ++i) {
        double s = 0.0;
        for (const auto& form : m_entries[i].forms)
            if (mayMatch(form, textNids, textHasLatin, minScore)) s = std::max(s, similarity(form, heard));
        if (s >= minScore) hits.push_back({ s, i });
    }
    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.score > b.score; });

    QStringList out;
    for (const Hit& h : hits) {
        if (out.size() >= maxWords) break;
        out << m_entries[h.order].text;
    }
    return out;
}
