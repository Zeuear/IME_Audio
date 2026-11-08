#pragma once
#include <QDateTime>
#include <QString>

class QTextDocument;

// @brief 实时转录稿子的追加规则：只在文档当前末尾追加、不记位置，录音时编辑前文也不影响；不拥有文档。
class TranscriptDocument
{
public:
    explicit TranscriptDocument(QTextDocument* document);

    // 开启后每句另起一行并带 [hh:mm:ss]。只影响之后追加的内容，已写进稿子的不回头重排
    void setTimestampsVisible(bool visible) { m_timestamps = visible; }
    bool timestampsVisible() const { return m_timestamps; }

    // 空白句忽略。中日文句子直接相连；西文句子之间补一个空格
    void appendUtterance(const QString& text, const QDateTime& time);

private:
    QTextDocument* m_document;
    bool m_timestamps = false;
};
