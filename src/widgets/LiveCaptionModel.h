#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

// @brief 实时字幕的内容模型：把流式识别的中间结果和正式转录的最终结果合并成一串可显示的句子，
//        不涉及绘制，LiveOverlay 按 segments() 渲染。
//
// 一句话的生命周期：Active（正在说）→ Pending（已切出送去转录，仍显示中间文本）→ Final（正式结果替换文本）。
// 转录队列是串行的，所以正式结果按先进先出对应 Pending。
class LiveCaptionModel : public QObject
{
    Q_OBJECT
public:
    enum class State { Active, Pending, Final };

    struct Sentence {
        QString text;
        State state = State::Active;
        qint64 stamp = 0;
    };

    struct Segment {
        QString text;
        bool isFinal = false;
    };

    static constexpr int kMaxSentences = 6;
    static constexpr qint64 kFinalLingerMs = 4000;
    static constexpr qint64 kPendingTimeoutMs = 60000;   // 云端后端可能很慢，超时放弃以免浅色文字残留

    explicit LiveCaptionModel(QObject *parent = nullptr);

    // 只有说话期间的中间结果才显示，避免静音噪声被识别成字
    void setSpeaking(bool speaking);
    // text 是本句累计全文；epoch 与当前不符的属于上一句，丢弃
    void setPartial(quint64 epoch, const QString &text);
    // 与 utteranceReady 一一对应
    void onUtteranceReady(qint64 nowMs = -1);
    // 与 transcriptionFinished 一一对应，空文本表示失败或无内容
    void onTranscribed(const QString &finalText, qint64 nowMs = -1);
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
    // 流式引擎据此重置识别流，并给后续中间结果打上新 epoch
    void epochChanged(quint64 epoch);

private:
    static qint64 resolveNow(qint64 nowMs);
    void touch();

    QVector<Sentence> m_sentences;
    quint64 m_epoch = 0;
    bool m_speaking = false;
    QTimer *m_expireTimer = nullptr;
};

namespace CaptionText {
// 全大写的英文转成句首大写，并压缩多余空白
QString normalizePartial(const QString &raw);
}
