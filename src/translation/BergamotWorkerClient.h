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
// 启动、发送、停止是虚函数，测试里换成假实现，不需要真实的 worker。
class BergamotWorkerClient : public ITranslationBackend {
    Q_OBJECT
public:
    // 路线和模型信息由外部提供（MainWin 接 TranslationModelInstaller），客户端不依赖下载器
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

    bool m_started = false;           // 已经请求启动
    bool m_ready = false;             // worker 已输出 READY
    QList<QByteArray> m_queue;        // READY 之前先排队
    QSet<QString> m_loaded;           // 已发过 L 的方向，形如 "en>de"
    QSet<qint64> m_pending;           // 等待响应的请求 id
};
