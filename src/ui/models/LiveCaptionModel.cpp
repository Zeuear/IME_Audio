#include "LiveCaptionModel.h"
#include <QDateTime>
#include <QRegularExpression>
#include <utility>

bool isCjk(QChar c)
{
    const ushort u = c.unicode();
    return (u >= 0x2E80 && u <= 0x9FFF) || (u >= 0xAC00 && u <= 0xD7AF)
        || (u >= 0xF900 && u <= 0xFAFF) || (u >= 0xFF00 && u <= 0xFFEF);
}
QString LiveCaptionModel::normalizePartial(const QString &raw)
{
    QString s = raw.simplified();
    if (s.isEmpty()) return s;

    // 英文流式模型输出全大写；仅处理纯英文文本，夹着中日韩文字时大写词多半是缩写，保持原样
    bool hasUpper = false, hasLower = false, hasCjk = false;
    for (QChar c : s) {
        if (c.isUpper()) hasUpper = true;
        else if (c.isLower()) hasLower = true;
        else if (isCjk(c)) hasCjk = true;
    }
    if (hasUpper && !hasLower && !hasCjk) {
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
    if (speaking && !m_held.text.isEmpty()) {
        const Held held = std::exchange(m_held, {});
        applyPartial(held.epoch, held.text);
    }
}

void LiveCaptionModel::setPartial(quint64 epoch, const QString &text)
{
    applyPartial(epoch, text);
}

void LiveCaptionModel::applyPartial(quint64 epoch, const QString &text)
{
    QString t = normalizePartial(text);
    if (m_filter && !t.isEmpty()) t = m_filter(t);

    // 流式解码落后于切句：句尾几个字常在切句之后才解出来，按 epoch 找回那一句继续更新
    for (int i = m_sentences.size() - 1; i >= 0; --i) {
        Sentence &s = m_sentences[i];
        if (s.epoch != epoch) continue;
        // 已被正式结果替换；流式识别偶尔会整个撤回成空串，保留上一次的文字以免闪烁
        if (s.state == State::Final || t.isEmpty() || s.text == t) return;
        s.text = t;
        touch();
        return;
    }
    if (epoch != m_epoch || t.isEmpty()) return;
    // VAD 判定开口比流式出字晚，且引擎只在文字变化时发结果：这段时间的结果先留着，确认在说话后补上，静音噪声不会进字幕
    if (!m_speaking) {
        m_held = { epoch, text };
        return;
    }
    m_sentences.clear();
    m_sentences.append({t, State::Active, resolveNow(-1), epoch});
    touch();
}

void LiveCaptionModel::onUtteranceReady(quint64 id, qint64 nowMs)
{
    const qint64 now = resolveNow(nowMs);
    if (!m_sentences.isEmpty() && m_sentences.last().state == State::Active) {
        m_sentences.last().state = State::Pending;
        m_sentences.last().stamp = now;
        m_sentences.last().id = id;
    } else {
        m_sentences.append({QString(), State::Pending, now, m_epoch, id});
    }
    ++m_epoch;
    m_held = {};
    emit epochChanged(m_epoch);
    touch();
}

void LiveCaptionModel::onTranscribed(quint64 id, const QString &finalText, qint64 nowMs)
{
    const qint64 now = resolveNow(nowMs);
    const QString t = finalText.trimmed();

    for (int i = 0; i < m_sentences.size(); ++i) {
        if (m_sentences[i].state != State::Pending || m_sentences[i].id != id) continue;
        if (t.isEmpty()) {
            m_sentences.removeAt(i);
        } else {
            m_sentences[i].text = t;
            m_sentences[i].state = State::Final;
            m_sentences[i].stamp = now;
            // 没有流式模型时句子只在出正式结果时出现：新一句出结果，更早的句子随之清掉
            m_sentences.remove(0, i);
        }
        touch();
        return;
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
    ++m_epoch;
    m_held = {};
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

QString LiveCaptionModel::pendingText() const
{
    if (m_sentences.isEmpty() || m_sentences.last().state != State::Pending) return {};
    return m_sentences.last().text;
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
