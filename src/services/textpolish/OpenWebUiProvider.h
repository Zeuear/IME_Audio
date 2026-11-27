#pragma once
#include "TextPolishProvider.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

class OpenWebUiProvider : public TextPolishProvider {
public:
    struct RetryPlan {
        bool retry = false;
        int delayMs = 0;
        QString error;
        int timeouts = 0;
    };

    static constexpr int kMaxAttempts = 4;
    static constexpr int kCloudTimeoutMs = 60 * 1000;
    static constexpr int kLoopbackTimeoutMs = 300 * 1000;
    static constexpr int kModelsTimeoutMs = 20 * 1000;
    static constexpr int kMaxModels = 5000;

    explicit OpenWebUiProvider(QString apiKey = {}, bool allowSelfSigned = false)
        : m_apiKey(std::move(apiKey)), m_allowSelfSigned(allowSelfSigned) {}

    bool supports(const TextPolishService::RequestParams& params) const override;
    bool canPolish() const override { return false; }
    bool canTranslate() const override { return true; }
    bool canTest() const override { return true; }
    bool canFetchModels() const override { return true; }

    void polish(const QString& inputText, const TextPolishService::RequestParams& params,
                QNetworkAccessManager* mgr, ResultCallback done) override;
    void fetchModels(const QString& baseUrl, QNetworkAccessManager* mgr, ModelsCallback done) override;
    void test(const TextPolishService::RequestParams& params, QNetworkAccessManager* mgr,
              TestCallback done) override;

    static QString normalizeBase(const QString& url, bool allowSelfSigned, QString* error);
    static bool isLoopback(const QString& url);
    static int requestTimeoutMs(const QString& url);
    static QByteArray buildChatBody(const QString& inputText, const TextPolishService::RequestParams& params);
    static QString extractText(const QByteArray& responseJson, QString* error);
    static QStringList parseModels(const QByteArray& responseJson, QString* error);
    static RetryPlan planRetry(int attempt, int timeouts, int httpStatus, int networkError,
                               const QByteArray& body, const QByteArray& retryAfter,
                               int timeoutMs, bool loopback);

    static void setRetryBaseDelayMs(int ms);

private:
    QString m_apiKey;
    bool m_allowSelfSigned = false;
};
