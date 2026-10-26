#pragma once
#include <QHash>
#include <QObject>
#include <QString>
#include <functional>

class ITranslationBackend;

// @brief 本地翻译的唯一入口：按 id 对回异步结果，超时兜底，同一请求只回调一次
class TranslationService : public QObject {
    Q_OBJECT
public:
    using ResultCallback = std::function<void(bool ok, const QString& text, const QString& error)>;

    static constexpr int kTranslateTimeoutMs = 30 * 1000;

    explicit TranslationService(ITranslationBackend* backend, QObject* parent = nullptr);

    void translate(const QString& text, const QString& from, const QString& to, ResultCallback cb);

    // 没有后端时翻译请求只会失败；调用方据此直接跳过翻译阶段，避免逐句报错
    bool hasBackend() const { return m_backend != nullptr; }

    // 仅测试使用：缩短超时，避免等 30 秒
    void setRequestTimeoutForTest(int ms) { m_timeoutMs = ms; }

private:
    struct Pending {
        ResultCallback cb;
    };

    void onTranslated(qint64 id, bool ok, const QString& text, const QString& error);
    void fail(qint64 id, const QString& error);
    void failAll(const QString& error);

    ITranslationBackend* m_backend = nullptr;
    QHash<qint64, Pending> m_pending;
    qint64 m_nextId = 1;
    int m_timeoutMs = kTranslateTimeoutMs;
};
