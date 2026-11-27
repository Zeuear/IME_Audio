#include "OllamaClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <memory>

#include "services/textpolish/OpenAiProvider.h"
#include "utils/Logger.h"

namespace {
// 检测服务只需本机回环，超过几秒还没回就是没在运行
constexpr int kProbeTimeoutMs = 3000;
constexpr int kRequestTimeoutMs = 15000;

QString normalized(const QString& name) {
    QString n = name.trimmed().toLower();
    // 最后一段带冒号才算标签；hf.co/... 这类路径里的 / 不影响
    const int slash = n.lastIndexOf('/');
    if (n.indexOf(':', slash + 1) < 0) n += QStringLiteral(":latest");
    return n;
}

QString describeNetworkError(QNetworkReply* reply, const QByteArray& body) {
    const QString serverError = QJsonDocument::fromJson(body).object().value("error").toString();
    if (!serverError.isEmpty()) return serverError;
    if (reply->error() == QNetworkReply::ConnectionRefusedError
        || reply->error() == QNetworkReply::HostNotFoundError)
        return QStringLiteral("无法连接 Ollama，请确认已安装并正在运行");
    return reply->errorString();
}
}

OllamaClient::OllamaClient(QNetworkAccessManager* manager, QObject* parent)
    : QObject(parent), m_manager(manager), m_baseUrl(OpenAiProvider::ollamaBaseUrl({}))
{
}

void OllamaClient::setBaseUrl(const QString& customUrl)
{
    m_baseUrl = OpenAiProvider::ollamaBaseUrl(customUrl);
}

bool OllamaClient::sameModel(const QString& a, const QString& b)
{
    return normalized(a) == normalized(b);
}

QList<OllamaClient::PullEvent> OllamaClient::takePullEvents(QByteArray& buffer)
{
    QList<PullEvent> events;
    int newline;
    while ((newline = buffer.indexOf('\n')) >= 0) {
        const QByteArray line = buffer.left(newline).trimmed();
        buffer.remove(0, newline + 1);
        if (line.isEmpty()) continue;
        const QJsonObject o = QJsonDocument::fromJson(line).object();
        if (o.isEmpty()) continue;
        PullEvent e;
        e.status = o.value("status").toString();
        e.error = o.value("error").toString();
        if (o.contains("completed")) e.completed = static_cast<qint64>(o.value("completed").toDouble());
        if (o.contains("total")) e.total = static_cast<qint64>(o.value("total").toDouble());
        e.success = e.status == QStringLiteral("success");
        events << e;
    }
    return events;
}

void OllamaClient::checkService()
{
    QNetworkRequest req(QUrl(m_baseUrl + QStringLiteral("api/version")));
    req.setTransferTimeout(kProbeTimeoutMs);
    QNetworkReply* reply = m_manager->get(req);
    const quint64 seq = ++m_checkSeq;
    connect(reply, &QNetworkReply::finished, this, [this, reply, seq]() {
        reply->deleteLater();
        if (seq != m_checkSeq) return;   // 之后又发起了新的检测，这个结果已过期
        if (reply->error() != QNetworkReply::NoError) {
            LOG_INFO(QString("Ollama not reachable at %1: %2").arg(m_baseUrl, reply->errorString()));
            emit serviceChecked(false, {});
            return;
        }
        const QString version = QJsonDocument::fromJson(reply->readAll()).object().value("version").toString();
        emit serviceChecked(true, version);
    });
}

void OllamaClient::refreshModels()
{
    QNetworkRequest req(QUrl(m_baseUrl + QStringLiteral("api/tags")));
    req.setTransferTimeout(kRequestTimeoutMs);
    QNetworkReply* reply = m_manager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit modelsListed(false, {});
            return;
        }
        emit modelsListed(true, OpenAiProvider::parseOllamaTags(reply->readAll()));
    });
}

bool OllamaClient::isPulling(const QString& name) const
{
    for (auto it = m_pulls.constBegin(); it != m_pulls.constEnd(); ++it) {
        if (it.value() && sameModel(it.key(), name)) return true;
    }
    return false;
}

void OllamaClient::pull(const QString& name)
{
    const QString model = name.trimmed();
    if (model.isEmpty() || isPulling(model)) return;

    QNetworkRequest req(QUrl(m_baseUrl + QStringLiteral("api/pull")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // 几个 GB 的下载不设整体超时；流式返回进度，连接断开时 Ollama 保留已下部分，下次续传
    const QByteArray body = QJsonDocument(QJsonObject{ {"model", model}, {"stream", true} }).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = m_manager->post(req, body);
    m_pulls.insert(model, reply);
    LOG_INFO(QString("Ollama pull started: %1").arg(model));

    auto buffer = std::make_shared<QByteArray>();
    auto lastError = std::make_shared<QString>();
    auto succeeded = std::make_shared<bool>(false);

    connect(reply, &QNetworkReply::readyRead, this, [this, reply, model, buffer, lastError, succeeded]() {
        buffer->append(reply->readAll());
        for (const PullEvent& e : takePullEvents(*buffer)) {
            if (!e.error.isEmpty()) {
                *lastError = e.error;
                continue;
            }
            if (e.success) *succeeded = true;
            emit pullProgress(model, e.completed, e.total, e.status);
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, model, buffer, lastError, succeeded]() {
        reply->deleteLater();
        m_pulls.remove(model);
        buffer->append(reply->readAll());
        buffer->append('\n');
        for (const PullEvent& e : takePullEvents(*buffer)) {
            if (!e.error.isEmpty()) *lastError = e.error;
            if (e.success) *succeeded = true;
        }
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            LOG_INFO(QString("Ollama pull cancelled: %1").arg(model));
            emit pullFinished(model, false, true, {});
            return;
        }
        if (*succeeded && lastError->isEmpty()) {
            LOG_INFO(QString("Ollama pull finished: %1").arg(model));
            emit pullFinished(model, true, false, {});
            return;
        }
        QString cause = *lastError;
        if (cause.isEmpty()) cause = describeNetworkError(reply, {});
        // Ollama 对不存在的名字返回 "file does not exist" / "pull model manifest: ..."
        if (cause.contains(QStringLiteral("does not exist"), Qt::CaseInsensitive)
            || cause.contains(QStringLiteral("manifest"), Qt::CaseInsensitive))
            cause = QStringLiteral("找不到模型「%1」，请检查名称和标签（%2）").arg(model, cause);
        LOG_WARN(QString("Ollama pull failed: %1: %2").arg(model, cause));
        emit pullFinished(model, false, false, cause);
    });
}

void OllamaClient::cancelPull(const QString& name)
{
    for (auto it = m_pulls.begin(); it != m_pulls.end(); ++it) {
        if (it.value() && sameModel(it.key(), name)) {
            it.value()->abort();
            return;
        }
    }
}

void OllamaClient::deleteModel(const QString& name)
{
    QNetworkRequest req(QUrl(m_baseUrl + QStringLiteral("api/delete")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(kRequestTimeoutMs);
    const QByteArray body = QJsonDocument(QJsonObject{ {"model", name} }).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = m_manager->sendCustomRequest(req, "DELETE", body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, name]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const QString cause = describeNetworkError(reply, body);
            LOG_WARN(QString("Ollama delete failed: %1: %2").arg(name, cause));
            emit modelDeleted(name, false, cause);
            return;
        }
        emit modelDeleted(name, true, {});
    });
}
