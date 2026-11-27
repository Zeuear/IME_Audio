#pragma once
#include <QHash>
#include <QString>
#include <optional>

#include "AiStagePlan.h"

// @brief 一句听写在云端 AI 阶段（润色、翻译）期间的状态，按识别序号索引
struct AiJob {
    QString source;
    QString current;
    AiStagePlan plan;
    QString target;
};

class AiJobTracker
{
public:
    void begin(quint64 seq, const QString& source, const AiStagePlan& plan, const QString& target) {
        m_jobs.insert(seq, AiJob{ source, source, plan, target });
    }
    const AiJob* find(quint64 seq) const {
        const auto it = m_jobs.constFind(seq);
        return it == m_jobs.constEnd() ? nullptr : &it.value();
    }
    bool active(quint64 seq) const { return m_jobs.contains(seq); }
    bool advance(quint64 seq, const QString& output) {
        const auto it = m_jobs.find(seq);
        if (it == m_jobs.end()) return false;
        it->current = output;
        return true;
    }
    // 结束这一句并取走状态；已经被取走（超时抢先或回调抢先）返回空
    std::optional<AiJob> take(quint64 seq) {
        const auto it = m_jobs.find(seq);
        if (it == m_jobs.end()) return std::nullopt;
        AiJob job = it.value();
        m_jobs.erase(it);
        return job;
    }
    int size() const { return int(m_jobs.size()); }

private:
    QHash<quint64, AiJob> m_jobs;
};
