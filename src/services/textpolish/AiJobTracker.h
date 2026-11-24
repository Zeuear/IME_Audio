#pragma once
#include <QHash>
#include <QString>
#include <optional>

#include "AiStagePlan.h"

// @brief 一句听写在云端 AI 阶段（润色、翻译）期间的状态，按识别序号索引
struct AiJob {
    QString source;     // 识别原文
    QString current;    // 当前阶段的输入：润色阶段 = 原文，翻译阶段 = 润色结果
    AiStagePlan plan;
    QString target;     // 翻译目标语言
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
    // 进入下一阶段：上一阶段的输出成为下一阶段的输入；已结束的句子返回 false
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
