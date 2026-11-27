#pragma once
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

// @brief 与本机 Ollama 服务通信：检测是否在运行、列出 / 下载（流式进度，可取消）/ 删除模型。
// 只做网络与协议解析，不碰界面；用户可见的错误原因用中文，经信号交给界面层统一提示。
class OllamaClient : public QObject
{
    Q_OBJECT
public:
    struct PullEvent {
        QString status;
        qint64 completed = -1;
        qint64 total = -1;
        QString error;
        bool success = false;
    };

    explicit OllamaClient(QNetworkAccessManager* manager, QObject* parent = nullptr);

    void setBaseUrl(const QString& customUrl);
    QString baseUrl() const { return m_baseUrl; }

    void checkService();   // 连续调用时只认最后一次的结果（地址改过后，旧地址迟到的失败不能盖掉新地址的成功）
    void refreshModels();
    void pull(const QString& name);
    void cancelPull(const QString& name);
    bool isPulling(const QString& name) const;
    void deleteModel(const QString& name);

    static QList<PullEvent> takePullEvents(QByteArray& buffer);
    static bool sameModel(const QString& a, const QString& b);

signals:
    void serviceChecked(bool running, const QString& version);
    void modelsListed(bool ok, const QStringList& models);
    void pullProgress(const QString& name, qint64 completed, qint64 total, const QString& status);
    // cancelled 为 true 时 error 为空，界面不当作失败
    void pullFinished(const QString& name, bool ok, bool cancelled, const QString& error);
    void modelDeleted(const QString& name, bool ok, const QString& error);

private:
    QNetworkAccessManager* m_manager;
    QString m_baseUrl;
    quint64 m_checkSeq = 0;
    QHash<QString, QPointer<QNetworkReply>> m_pulls;
};
