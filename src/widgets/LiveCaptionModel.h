#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>
#include <functional>

// @brief 实时字幕的内容模型
//
// 一句话的生命周期：Active（正在说）→ Pending（已切出送去转录，仍显示中间文本）→ Final（正式结果替换文本）。
class LiveCaptionModel : public QObject
{
    Q_OBJECT
public:
    enum class State { Active, Pending, Final };

    struct Sentence {
        QString text;
        State state = State::Active;
        qint64 stamp = 0;
        quint64 epoch = 0;
        quint64 id = 0;   // 切句后才有
    };

    struct Segment {
        QString text;
        bool isFinal = false;
    };

    static constexpr int kMaxSentences = 6;
    static constexpr qint64 kFinalLingerMs = 4000;
    static constexpr qint64 kPendingTimeoutMs = 60000;

    explicit LiveCaptionModel(QObject *parent = nullptr);

    // 中间结果显示前的加工（如术语替换），让浅色字和正式结果的专有名词一致
    void setTextFilter(std::function<QString(const QString &)> filter) { m_filter = std::move(filter); }
    void setSpeaking(bool speaking);
    void setPartial(quint64 epoch, const QString &text);
    void onUtteranceReady(quint64 id, qint64 nowMs = -1);
    void onTranscribed(quint64 id, const QString &finalText, qint64 nowMs = -1);
    // nowMs 供测试传入伪造时间；返回内容是否变化
    bool expire(qint64 nowMs = -1);
    void clear();

    QVector<Segment> segments() const;
    QString plainText() const;
    bool isEmpty() const;
    quint64 epoch() const { return m_epoch; }

    // 中日韩文字之间不加空格
    static QString separatorBetween(const QString &prev, const QString &next);

signals:
    void changed();
    // 流式引擎据此重置识别流
    void epochChanged(quint64 epoch);

private:
    static qint64 resolveNow(qint64 nowMs);
    void touch();
    void applyPartial(quint64 epoch, const QString &text);

    QVector<Sentence> m_sentences;
    quint64 m_epoch = 0;
    bool m_speaking = false;
    // 尚未开口时收到的最新一条中间结果原文；只保留最新，换句时作废
    struct Held { quint64 epoch = 0; QString text; };
    Held m_held;
    std::function<QString(const QString &)> m_filter;
    QTimer *m_expireTimer = nullptr;
};

namespace CaptionText {
// 全大写的英文转成句首大写，并压缩多余空白
QString normalizePartial(const QString &raw);
}
