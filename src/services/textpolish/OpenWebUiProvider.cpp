#include "OpenWebUiProvider.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QTimer>

#include "OpenAiProvider.h"
#include "TextPolishPrompts.h"
#include "config/AppConfig.h"
#include "utils/Logger.h"

namespace {

int& retryBaseDelayMs() {
    static int value = 1000;
    return value;
}

struct Job {
    QUrl url;
    QByteArray body;
    QByteArray key;
    bool selfSigned = false;
    bool loopback = false;
    int timeoutMs = 0;
};

QString connectionMessage(int networkError, bool loopback) {
    switch (networkError) {
    case QNetworkReply::SslHandshakeFailedError:
        return QStringLiteral("HTTPS 证书验证失败，请配置受信任证书")
            + (loopback ? QStringLiteral("；若是你自己的本地服务，可勾选“允许本机自签名证书”") : QString());
    case QNetworkReply::ConnectionRefusedError:
        return QStringLiteral("连接被拒绝，请检查服务是否启动以及端口是否正确");
    default:
        return QStringLiteral("无法连接 API，请检查地址、端口和网络（请求超时或连接中断）");
    }
}

bool isTimeout(int networkError) {
    return networkError == QNetworkReply::TimeoutError || networkError == QNetworkReply::OperationCanceledError;
}

QNetworkRequest makeRequest(const QUrl& url, const QByteArray& key, int timeoutMs) {
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    if (!key.isEmpty()) req.setRawHeader("Authorization", "Bearer " + key);
    req.setTransferTimeout(timeoutMs);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    return req;
}

void ignoreSslErrorsIf(QNetworkReply* reply, bool allow) {
    if (!allow) return;
    QObject::connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError>&) {
        reply->ignoreSslErrors();
    });
}

void post(QNetworkAccessManager* mgr, const Job& job, int attempt, int timeouts, TextPolishProvider::ResultCallback done) {
    QNetworkReply* reply = mgr->post(makeRequest(job.url, job.key, job.timeoutMs), job.body);
    ignoreSslErrorsIf(reply, job.selfSigned);
    QObject::connect(reply, &QNetworkReply::finished, mgr, [mgr, reply, job, attempt, timeouts, done]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (reply->error() == QNetworkReply::NoError && status < 300) {
            QString error;
            const QString text = OpenWebUiProvider::extractText(body, &error);
            done(error.isEmpty(), text, error);
            return;
        }
        const auto plan = OpenWebUiProvider::planRetry(attempt, timeouts, status, int(reply->error()), body,
                                                       reply->rawHeader("Retry-After"), job.timeoutMs, job.loopback);
        if (!plan.retry) {
            done(false, {}, plan.error);
            return;
        }
        LOG_WARN(QString("Open WebUI request failed (attempt %1, HTTP %2, net %3), retrying in %4 ms")
                     .arg(attempt + 1).arg(status).arg(int(reply->error())).arg(plan.delayMs));
        QTimer::singleShot(plan.delayMs, mgr, [mgr, job, attempt, plan, done]() {
            post(mgr, job, attempt + 1, plan.timeouts, done);
        });
    });
}

}

bool OpenWebUiProvider::supports(const TextPolishService::RequestParams& params) const {
    return params.aiEngine == kOpenWebUiEngine;
}

void OpenWebUiProvider::setRetryBaseDelayMs(int ms) {
    retryBaseDelayMs() = ms;
}

bool OpenWebUiProvider::isLoopback(const QString& url) {
    const QString host = QUrl(url.trimmed()).host();
    if (host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0) return true;
    QHostAddress address;
    return address.setAddress(host) && address.isLoopback();
}

int OpenWebUiProvider::requestTimeoutMs(const QString& url) {
    return isLoopback(url) ? kLoopbackTimeoutMs : kCloudTimeoutMs;
}

QString OpenWebUiProvider::normalizeBase(const QString& url, bool allowSelfSigned, QString* error) {
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return QString();
    };
    QString text = url.trimmed();
    if (text.isEmpty()) {
        return fail(QStringLiteral("使用 Open WebUI 引擎时，必须填写 API 地址（如 http://localhost:3000）"));
    }
    while (text.endsWith('/')) text.chop(1);
    const QUrl parsed(text);
    const bool schemeOk = parsed.scheme() == QStringLiteral("http") || parsed.scheme() == QStringLiteral("https");
    if (!parsed.isValid() || !schemeOk || parsed.host().isEmpty() || !parsed.userInfo().isEmpty()
        || parsed.hasQuery() || parsed.hasFragment()) {
        return fail(QStringLiteral("API 地址必须是有效的 HTTP(S) 地址，不能包含账号、查询参数或片段"));
    }
    // 自签名开关默认开启，远程地址不能因此报错；是否真正跳过校验由 isLoopback 决定，不是回环一律严格校验
    const QString suffix = QStringLiteral("/chat/completions");
    if (text.endsWith(suffix)) text.chop(suffix.size());
    while (text.endsWith('/')) text.chop(1);
    if (QUrl(text).path().isEmpty()) text += QStringLiteral("/api");
    return text;
}

QByteArray OpenWebUiProvider::buildChatBody(const QString& inputText, const TextPolishService::RequestParams& params) {
    TextPolishService::RequestParams p = params;
    p.translateOnly = true;
    const QJsonObject root{
        { "model", params.model },
        { "messages", OpenAiProvider::buildMessages(inputText, p) },
        { "temperature", 0.0 },
    };
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QString OpenWebUiProvider::extractText(const QByteArray& responseJson, QString* error) {
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return QString();
    };
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(responseJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return fail(QStringLiteral("API 响应不是兼容的聊天结果"));
    }
    const QJsonObject root = doc.object();
    if (root.contains("error")) {
        const QJsonValue e = root.value("error");
        const QString message = e.isObject() ? e.toObject().value("message").toString() : e.toString();
        return fail(message.isEmpty() ? QStringLiteral("API 返回了错误") : message);
    }
    const QJsonArray choices = root.value("choices").toArray();
    if (choices.isEmpty()) return fail(QStringLiteral("无候选结果"));
    const QJsonObject choice = choices.first().toObject();
    if (choice.value("finish_reason").toString() == QStringLiteral("length")) {
        return fail(QStringLiteral("模型输出被截断"));
    }
    const QString text = TextPolishPrompts::stripThinking(
        choice.value("message").toObject().value("content").toString());
    if (text.isEmpty()) return fail(QStringLiteral("模型没有返回正文"));
    return text;
}

QStringList OpenWebUiProvider::parseModels(const QByteArray& responseJson, QString* error) {
    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return QStringList();
    };
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(responseJson, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return fail(QStringLiteral("模型接口未返回 JSON，请检查 API 路径是否正确"));
    }
    const QJsonValue data = doc.object().value("data");
    if (!data.isArray()) return fail(QStringLiteral("模型接口应返回包含 data 数组的兼容响应"));
    const QJsonArray items = data.toArray();
    if (items.size() > kMaxModels) return fail(QStringLiteral("模型列表超过 5000 项上限"));
    QStringList ids;
    for (const QJsonValue& item : items) {
        const QString id = item.toObject().value("id").toString().trimmed();
        if (id.isEmpty()) return fail(QStringLiteral("模型列表缺少有效模型 ID"));
        if (!ids.contains(id)) ids << id;
    }
    if (ids.isEmpty()) return fail(QStringLiteral("服务返回空模型列表，可手动填写模型名称"));
    return ids;
}

OpenWebUiProvider::RetryPlan OpenWebUiProvider::planRetry(int attempt, int timeouts, int httpStatus, int networkError,
                                                          const QByteArray& body, const QByteArray& retryAfter,
                                                          int timeoutMs, bool loopback) {
    RetryPlan plan;
    plan.timeouts = timeouts;
    const bool last = attempt >= kMaxAttempts - 1;
    const int backoff = retryBaseDelayMs() * (1 << attempt);

    if (httpStatus > 0) {
        const QByteArray lower = body.left(16384).toLower();
        static const QList<QByteArray> kContextWords = {
            "context_length", "maximum context", "too many tokens", "context window" };
        bool overflow = httpStatus == 413;
        if (httpStatus == 400) {
            for (const QByteArray& word : kContextWords) overflow |= lower.contains(word);
        }
        if (overflow) {
            plan.error = QStringLiteral("请求超过模型上下文限制");
            return plan;
        }
        if (httpStatus == 401 || httpStatus == 403) {
            plan.error = QStringLiteral("API 认证或权限失败，请检查密钥和模型权限");
            return plan;
        }
        static const QList<int> kRetryable = { 408, 429, 500, 502, 503, 504 };
        if (!kRetryable.contains(httpStatus)) {
            plan.error = QStringLiteral("API 请求失败（HTTP %1），请检查地址与模型").arg(httpStatus);
            return plan;
        }
        if (last) {
            plan.error = QStringLiteral("API 临时故障，重试 3 次后仍未成功");
            return plan;
        }
        bool digits = false;
        const int seconds = QString::fromLatin1(retryAfter).trimmed().toInt(&digits);
        plan.delayMs = digits && seconds >= 0 ? qMin(seconds, 60) * 1000 : backoff;
        plan.retry = true;
        return plan;
    }

    if (isTimeout(networkError)) {
        plan.timeouts = timeouts + 1;
        if (plan.timeouts >= 2) {
            plan.error = QStringLiteral("模型响应超时：%1 秒内没有完成（已重试 1 次）").arg(timeoutMs / 1000);
            return plan;
        }
        plan.retry = true;
        plan.delayMs = backoff;
        return plan;
    }

    const bool permanent = networkError == QNetworkReply::SslHandshakeFailedError
        || networkError == QNetworkReply::ConnectionRefusedError
        || networkError == QNetworkReply::HostNotFoundError;
    if (permanent || last) {
        plan.error = connectionMessage(networkError, loopback);
        return plan;
    }
    plan.retry = true;
    plan.delayMs = backoff;
    return plan;
}

void OpenWebUiProvider::polish(const QString& inputText, const TextPolishService::RequestParams& params,
                               QNetworkAccessManager* mgr, ResultCallback done) {
    if (!params.translateOnly) {
        done(true, inputText, {});
        return;
    }
    QString error;
    const QString base = normalizeBase(params.customUrl, params.allowSelfSigned, &error);
    if (base.isEmpty()) {
        done(false, {}, error);
        return;
    }
    if (params.model.trimmed().isEmpty()) {
        done(false, {}, QStringLiteral("请选择或填写 Open WebUI 模型名称"));
        return;
    }
    Job job;
    job.url = QUrl(base + QStringLiteral("/chat/completions"));
    job.body = buildChatBody(inputText, params);
    job.key = m_apiKey.toUtf8();
    job.loopback = isLoopback(base);
    job.selfSigned = params.allowSelfSigned && job.loopback;
    job.timeoutMs = requestTimeoutMs(base);
    post(mgr, job, 0, 0, std::move(done));
}

void OpenWebUiProvider::fetchModels(const QString& baseUrl, QNetworkAccessManager* mgr, ModelsCallback done) {
    QString error;
    const QString base = normalizeBase(baseUrl, m_allowSelfSigned, &error);
    if (base.isEmpty()) {
        done(false, {}, error);
        return;
    }
    const bool loopback = isLoopback(base);
    const bool selfSigned = m_allowSelfSigned && loopback;
    const int timeoutMs = qMin(requestTimeoutMs(base), kModelsTimeoutMs);

    QNetworkReply* reply = mgr->get(makeRequest(QUrl(base + QStringLiteral("/models")), m_apiKey.toUtf8(), timeoutMs));
    ignoreSslErrorsIf(reply, selfSigned);
    QObject::connect(reply, &QNetworkReply::finished, mgr, [reply, loopback, done]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status >= 300) {
            if (status == 401 || status == 403) {
                done(false, {}, QStringLiteral("模型列表认证失败，请检查 API Key"));
            } else if (status > 0) {
                done(false, {}, QStringLiteral("模型列表请求失败（HTTP %1），请检查 API 地址；也可手动填写模型").arg(status));
            } else {
                done(false, {}, connectionMessage(int(reply->error()), loopback));
            }
            return;
        }
        QString parseError;
        const QStringList ids = parseModels(reply->readAll(), &parseError);
        done(parseError.isEmpty(), ids, parseError);
    });
}

void OpenWebUiProvider::test(const TextPolishService::RequestParams& params, QNetworkAccessManager* mgr,
                             TestCallback done) {
    fetchModels(params.customUrl, mgr, [done](bool ok, QStringList models, QString error) {
        done(ok, ok ? QStringLiteral("Open WebUI 连接成功，已获取 %1 个模型").arg(models.size()) : error);
    });
}
