#pragma once

#include "ITranslationBackend.h"
#include "LanguageRouting.h"

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <functional>

class QProcess;

// @brief 本地翻译后端：管理 translate_worker 进程，用行协议（tools/translate_worker/WorkerProtocol.h）收发。
// 进程按需启动；崩溃或退出时所有未完成请求立刻以失败返回，下一次翻译会自动重启。
class BergamotWorkerClient : public ITranslationBackend {
    Q_OBJECT
public:
    struct RouteProvider {
        std::function<TranslationRoute(const QString& from, const QString& to)> plan;
        std::function<bool(const QString& from, const QString& to)> isInstalled;
        std::function<QString(const QString& from, const QString& to)> configPath;
    };

    BergamotWorkerClient(RouteProvider provider, const QString& workerPath, QObject* parent = nullptr);
    ~BergamotWorkerClient() override;

    void translate(qint64 id, const QString& text, const QString& from, const QString& to) override;
    bool isAvailable(const QString& from, const QString& to) const override;
    bool isUsable() const override;

protected:
    virtual void startWorker();
    virtual void sendLine(const QByteArray& line);
    virtual void stopWorker();

    void handleLine(const QString& line);
    void handleWorkerGone(const QString& reason);

private:
    void enqueue(const QByteArray& line);
    void flushQueue();
    void failAll(const QString& cause);
    static QString friendlyError(const QString& code, const QString& message);

    RouteProvider m_provider;
    QString m_workerPath;
    QProcess* m_process = nullptr;
    QByteArray m_stdoutBuffer;

    bool m_started = false;
    bool m_ready = false;
    QList<QByteArray> m_queue;
    QSet<QString> m_loaded;           // 已发过 L 的方向，形如 "en>de"
    QSet<qint64> m_pending;
};
