#include "LiveCaptionModel.h"
#include <QDateTime>
#include <QRegularExpression>

namespace {
bool isCjk(QChar c)
{
    const ushort u = c.unicode();
    return (u >= 0x2E80 && u <= 0x9FFF) || (u >= 0xAC00 && u <= 0xD7AF)
        || (u >= 0xF900 && u <= 0xFAFF) || (u >= 0xFF00 && u <= 0xFFEF);
}
} // namespace

QString CaptionText::normalizePartial(const QString &raw)
{
    QString s = raw.simplified();
    if (s.isEmpty()) return s;

    // 英文流式模型输出全大写；只处理"有大写、没小写"的文本
    bool hasUpper = false, hasLower = false;
    for (QChar c : s) {
        if (c.isUpper()) hasUpper = true;
        else if (c.isLower()) hasLower = true;
    }
    if (hasUpper && !hasLower) {
        s = s.toLower();
        static const QRegularExpression kLoneI(QStringLiteral("\\bi\\b"));
        s.replace(kLoneI, QStringLiteral("I"));
        for (int i = 0; i < s.size(); ++i) {
            if (s[i].isLetter()) { s[i] = s[i].toUpper(); break; }
        }
    }
    return s;
}

LiveCaptionModel::LiveCaptionModel(QObject *parent)
    : QObject(parent)
{
    m_expireTimer = new QTimer(this);
    m_expireTimer->setInterval(500);
    connect(m_expireTimer, &QTimer::timeout, this, [this]() { expire(); });
}

qint64 LiveCaptionModel::resolveNow(qint64 nowMs)
{
    return nowMs >= 0 ? nowMs : QDateTime::currentMSecsSinceEpoch();
}

void LiveCaptionModel::touch()
{
    if (m_sentences.isEmpty()) m_expireTimer->stop();
    else if (!m_expireTimer->isActive()) m_expireTimer->start();
    emit changed();
}

void LiveCaptionModel::setSpeaking(bool speaking)
{
    m_speaking = speaking;
}

void LiveCaptionModel::setPartial(quint64 epoch, const QString &text)
{
    if (epoch != m_epoch || !m_speaking) return;
    const QString t = CaptionText::normalizePartial(text);

    if (!m_sentences.isEmpty() && m_sentences.last().state == State::Active) {
        Sentence &cur = m_sentences.last();
        if (cur.text == t) return;
        // 流式识别偶尔会整个撤回成空串，保留上一次的文字以免闪烁
        if (t.isEmpty()) return;
        cur.text = t;
        touch();
        return;
    }
    if (t.isEmpty()) return;
    m_sentences.append({t, State::Active, resolveNow(-1)});
    while (m_sentences.size() > kMaxSentences) m_sentences.removeFirst();
    touch();
}

void LiveCaptionModel::onUtteranceReady(qint64 nowMs)
{
    const qint64 now = resolveNow(nowMs);
    if (!m_sentences.isEmpty() && m_sentences.last().state == State::Active) {
        m_sentences.last().state = State::Pending;
        m_sentences.last().stamp = now;
    } else {
        // 没有中间文本（如该语言无流式模型）也要占位，否则正式结果会和别的句子错位
        m_sentences.append({QString(), State::Pending, now});
    }
    ++m_epoch;
    emit epochChanged(m_epoch);
    touch();
}

void LiveCaptionModel::onTranscribed(const QString &finalText, qint64 nowMs)
{
    const qint64 now = resolveNow(nowMs);
    const QString t = finalText.trimmed();

    for (int i = 0; i < m_sentences.size(); ++i) {
        if (m_sentences[i].state != State::Pending) continue;
        if (t.isEmpty()) {
            m_sentences.removeAt(i);
        } else {
            m_sentences[i].text = t;
            m_sentences[i].state = State::Final;
            m_sentences[i].stamp = now;
        }
        touch();
        return;
    }
    // 没有待定句（如收到上一轮遗留的结果）时直接追加
    if (!t.isEmpty()) {
        m_sentences.append({t, State::Final, now});
        while (m_sentences.size() > kMaxSentences) m_sentences.removeFirst();
        touch();
    }
}

bool LiveCaptionModel::expire(qint64 nowMs)
{
    const qint64 now = resolveNow(nowMs);
    const int before = m_sentences.size();
    for (int i = m_sentences.size() - 1; i >= 0; --i) {
        const Sentence &s = m_sentences[i];
        const bool stale = (s.state == State::Final && now - s.stamp >= kFinalLingerMs)
                        || (s.state == State::Pending && now - s.stamp >= kPendingTimeoutMs);
        if (stale) m_sentences.removeAt(i);
    }
    const bool changed = m_sentences.size() != before;
    if (changed) touch();
    else if (m_sentences.isEmpty()) m_expireTimer->stop();
    return changed;
}

void LiveCaptionModel::clear()
{
    const bool had = !m_sentences.isEmpty();
    m_sentences.clear();
    m_speaking = false;
    // 让还在队列里的上一轮中间结果作废
    ++m_epoch;
    emit epochChanged(m_epoch);
    if (had) touch();
    else m_expireTimer->stop();
}

QString LiveCaptionModel::separatorBetween(const QString &prev, const QString &next)
{
    if (prev.isEmpty() || next.isEmpty()) return QString();
    if (isCjk(prev.back()) || isCjk(next.front())) return QString();
    return QStringLiteral(" ");
}

QVector<LiveCaptionModel::Segment> LiveCaptionModel::segments() const
{
    QVector<Segment> out;
    QString prevText;
    for (const Sentence &s : m_sentences) {
        if (s.text.isEmpty()) continue;
        const bool fin = s.state == State::Final;
        const QString sep = separatorBetween(prevText, s.text);
        if (!out.isEmpty() && out.last().isFinal == fin) {
            out.last().text += sep + s.text;
        } else {
            out.append({sep + s.text, fin});
        }
        prevText = s.text;
    }
    return out;
}

QString LiveCaptionModel::plainText() const
{
    QString all;
    for (const Segment &seg : segments()) all += seg.text;
    return all;
}

bool LiveCaptionModel::isEmpty() const
{
    for (const Sentence &s : m_sentences) {
        if (!s.text.isEmpty()) return false;
    }
    return true;
}
