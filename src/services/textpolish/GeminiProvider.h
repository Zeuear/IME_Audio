#pragma once
#include "TextPolishProvider.h"

class GeminiProvider : public TextPolishProvider {
public:
    bool supports(const TextPolishService::RequestParams& params) const override {
        return params.aiEngine == 0;
    }
    bool canFetchModels() const override { return true; }
    bool canTest() const override { return true; }

    void polish(const QString& inputText,
                const TextPolishService::RequestParams& params,
                QNetworkAccessManager* mgr,
                ResultCallback done) override;

    void test(const TextPolishService::RequestParams& params,
              QNetworkAccessManager* mgr,
              TestCallback done) override;

    void fetchModels(const QString& baseUrl,
        QNetworkAccessManager* mgr,
        ModelsCallback done) override;

    void transcribe(const QByteArray& wavBytes,
        const TextPolishService::RequestParams& params,
        QNetworkAccessManager* mgr,
        ResultCallback done);

private:
    static QString buildEndpoint(const TextPolishService::RequestParams& params);
    static QByteArray buildTextRequestBody(const QString& text, const TextPolishService::RequestParams& params);
    static QByteArray buildAudioRequestBody(const QByteArray& audioBase64, const TextPolishService::RequestParams& params);
    static QString buildPolishPrompt(const QString& inputText, const TextPolishService::RequestParams& params);
    static QString buildTranscribePromptText(const TextPolishService::RequestParams& params);
    static QString extractTextFromResponse(const QByteArray& responseJson, QString* error);
};
