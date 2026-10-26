#include "TranslationService.h"
#include "translation/ITranslationBackend.h"
#include "utils/Logger.h"

#include <QTimer>
#include <utility>

TranslationService::TranslationService(ITranslationBackend* backend, QObject* parent)
    : QObject(parent), m_backend(backend)
{
    if (!m_backend) return;

    connect(m_backend, &ITranslationBackend::translated, this, &TranslationService::onTranslated);
    // worker 崩溃/断开时不能让调用方永远等下去
    connect(m_backend, &QObject::destroyed, this, [this]() {
        m_backend = nullptr;
        failAll(QStringLiteral("翻译后端已断开"));
    });
}

void TranslationService::translate(const QString& text, const QString& from, const QString& to, ResultCallback cb) {
    if (!m_backend) {
        cb(false, {}, QStringLiteral("翻译后端不可用"));
        return;
    }
    if (!m_backend->isAvailable(from, to)) {
        cb(false, {}, QStringLiteral("翻译模型未安装"));
        return;
    }

    const qint64 id = m_nextId++;
    // 先登记再调用：后端如果同步发结果，也要能按 id 找到
    m_pending.insert(id, Pending{ std::move(cb) });
    m_backend->translate(id, text, from, to);

    QTimer::singleShot(m_timeoutMs, this, [this, id]() {
        fail(id, QStringLiteral("翻译超时"));
    });
}

void TranslationService::onTranslated(qint64 id, bool ok, const QString& text, const QString& error) {
    auto it = m_pending.find(id);
    if (it == m_pending.end()) return;   // 已超时或重复结果

    ResultCallback cb = std::move(it->cb);
    m_pending.erase(it);

    if (ok) cb(true, text, {});
    else cb(false, {}, error);
}

void TranslationService::fail(qint64 id, const QString& error) {
    auto it = m_pending.find(id);
    if (it == m_pending.end()) return;

    ResultCallback cb = std::move(it->cb);
    m_pending.erase(it);
    cb(false, {}, error);
}

void TranslationService::failAll(const QString& error) {
    const auto pending = std::move(m_pending);
    m_pending.clear();
    for (const Pending& p : pending) p.cb(false, {}, error);
}
