#pragma once
#include "TextPolishProvider.h"

class OpenAiProvider : public TextPolishProvider {
public:
    bool supports(const TextPolishService::RequestParams& params) const override {
        return params.aiEngine == 1 || params.aiEngine == 2;
    }
    bool canFetchModels() const override { return true; }
    bool canTest() const override { return true; }

    static QStringList parseOllamaTags(const QByteArray& json);
    static bool isValidModelsResponse(const QByteArray& json);

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
    static QByteArray buildOpenAiChatBody(const QString& inputText, const TextPolishService::RequestParams& params);
    static QString buildOpenAiEndpoint(const TextPolishService::RequestParams& params);
    static QString extractOpenAiTextFromResponse(const QByteArray& responseJson, QString* error);
};
