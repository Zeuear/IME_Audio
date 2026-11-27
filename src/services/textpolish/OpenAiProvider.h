#pragma once
#include "TextPolishProvider.h"

#include <QJsonArray>
#include <utility>

// @brief OpenAI 兼容服务（引擎 1）与 Ollama（引擎 2）。
// Ollama 走原生 /api/chat：能关思考模式、让模型常驻；OpenAI 兼容服务走 /v1/chat/completions。
// 两者共用同一套消息构造：润色（system + fewshot + user）、只翻译（system + user），
// 翻译专用模型只发一条按其训练格式写的 user 消息。
class OpenAiProvider : public TextPolishProvider {
public:
    explicit OpenAiProvider(int aiEngine = 2, QString apiKey = {})
        : m_engine(aiEngine), m_apiKey(std::move(apiKey)) {}

    bool supports(const TextPolishService::RequestParams& params) const override {
        return params.aiEngine == 1 || params.aiEngine == 2;
    }
    bool canFetchModels() const override { return true; }
    bool canTest() const override { return true; }

    static QStringList parseOllamaTags(const QByteArray& json);
    static QStringList parseOpenAiModels(const QByteArray& json);
    static bool isValidModelsResponse(const QByteArray& json);

    static QJsonArray buildMessages(const QString& inputText, const TextPolishService::RequestParams& params);
    static QByteArray buildOpenAiChatBody(const QString& inputText, const TextPolishService::RequestParams& params);
    static QByteArray buildOllamaChatBody(const QString& inputText, const TextPolishService::RequestParams& params,
                                          bool withThink = true);
    static QString extractOpenAiTextFromResponse(const QByteArray& responseJson, QString* error);
    static QString extractOllamaChatText(const QByteArray& responseJson, QString* error);
    // 用户可能把 Ollama 地址填成 .../v1 或 .../api/chat，统一还原成服务根地址（以 / 结尾）
    static QString ollamaBaseUrl(const QString& customUrl);

    void polish(const QString& inputText,
                const TextPolishService::RequestParams& params,
                QNetworkAccessManager* mgr,
                ResultCallback done) override;

    void fetchModels(const QString& baseUrl,
                     QNetworkAccessManager* mgr,
                     ModelsCallback done) override;

    void test(const TextPolishService::RequestParams& params,
              QNetworkAccessManager* mgr,
              TestCallback done) override;

private:
    static QString buildOpenAiEndpoint(const TextPolishService::RequestParams& params);
    // 静态：provider 每次请求临时创建、发出后即销毁，回调里不能再碰 this
    static void polishOllama(const QString& inputText, const TextPolishService::RequestParams& params,
                      QNetworkAccessManager* mgr, ResultCallback done, bool withThink);

    int m_engine = 2;
    QString m_apiKey;
};
