#pragma once
#include <QList>
#include <QMap>
#include <utility>

// @brief 让并发的异步结果按提交顺序放行
template <typename T>
class OrderedResults
{
public:
    quint64 reserve() { return m_next++; }

    // 返回 false 表示该序号已完成过或不存在，结果被丢弃
    bool complete(quint64 seq, T value)
    {
        if (seq < m_head || seq >= m_next || m_done.contains(seq)) return false;
        m_done.insert(seq, std::move(value));
        return true;
    }

    bool isPending(quint64 seq) const
    {
        return seq >= m_head && seq < m_next && !m_done.contains(seq);
    }

    QList<T> takeReady()
    {
        QList<T> ready;
        while (m_done.contains(m_head)) ready.append(m_done.take(m_head++));
        return ready;
    }

private:
    quint64 m_next = 0;
    quint64 m_head = 0;
    QMap<quint64, T> m_done;
};
