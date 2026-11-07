#include "TranscriptDocument.h"
#include <QTextCursor>
#include <QTextDocument>

namespace {
// 中日韩文字、全角标点：相邻句子之间不需要空格
bool isCjk(QChar c)
{
    return c.unicode() >= 0x2E80;
}

bool isLineBreak(QChar c)
{
    return c == QChar::ParagraphSeparator || c == QLatin1Char('\n');
}
} // namespace

TranscriptDocument::TranscriptDocument(QTextDocument* document)
    : m_document(document)
{
}

void TranscriptDocument::appendUtterance(const QString& text, const QDateTime& time)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) return;

    QTextCursor cursor(m_document);
    cursor.movePosition(QTextCursor::End);

    // 末尾字符取文档此刻的真实内容：用户删光、换行、改字之后规则依然成立
    const int count = m_document->characterCount();
    const QChar last = count >= 2 ? m_document->characterAt(count - 2) : QChar();
    const bool atLineStart = last.isNull() || isLineBreak(last);

    if (m_timestamps) {
        if (!atLineStart) cursor.insertBlock();
        cursor.insertText(QStringLiteral("[%1] ").arg(time.toString(QStringLiteral("hh:mm:ss"))) + t);
        return;
    }

    const bool needSpace = !atLineStart && !last.isSpace() && !isCjk(last) && !isCjk(t.at(0));
    cursor.insertText(needSpace ? QLatin1Char(' ') + t : t);
}
