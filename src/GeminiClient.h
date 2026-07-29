#pragma once
#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

class GeminiClient : public QObject {
    Q_OBJECT
public:
    struct RequestParams {
        QString apiKey;
        QString model;
        QString customUrl;
        QString targetLang;
        QString style;
        QString customPrompt;
        QString replaceRules;
        QString aiVocab;
        int aiEngine = 0; // 0 = Gemini, 1 = OpenAI兼容(本地/自訂)
    };

    explicit GeminiClient(QNetworkAccessManager* networkManager, QObject *parent = nullptr);

    void transcribeAudio(const QByteArray& wavBytes, const RequestParams& params);
    void polishText(const QString &inputText, const RequestParams &params);
    void testConnection(const RequestParams &params);

signals:
    void transcribeFinished(bool success, const QString &text, const QString &error);
    void polishFinished(bool success, const QString &text, const QString &error);
    void connectionTested(bool success, const QString &message);

private:
    QString buildEndpoint(const RequestParams& params, bool forGenerateContent) const;
    QString buildOpenAiEndpoint(const RequestParams& params) const;
    QByteArray buildAudioRequestBody(const QByteArray& audioBase64, const RequestParams& params) const;
    QByteArray buildTextRequestBody(const QString& text, const RequestParams& params) const;
    QByteArray buildOpenAiChatBody(const QString& inputText, const RequestParams& params) const;
    QString extractTextFromResponse(const QByteArray& responseJson, QString* error) const;
    QString extractOpenAiTextFromResponse(const QByteArray& responseJson, QString* error) const;

    // prompt
    QString loadSystemPrompt(const QString& filename, const QString& defaultPrompt) const;
    QString styleInstruction(const QString& style) const;
    QString buildPolishPrompt(const QString& inputText, const RequestParams& params) const;
    QString buildTranscribePromptText(const RequestParams& params) const;
    QStringList buildFewshotMessages(const QString& fewshotText) const; 
    QString autocompleteApiUrl(const QString& input) const;

    void ensurePromptsDirInited();

    QNetworkAccessManager *m_manager;
};