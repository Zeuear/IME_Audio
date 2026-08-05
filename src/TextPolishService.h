#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>

class TextPolishService : public QObject {
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
        int aiEngine = 0; // 0 = Gemini, 1 = OpenAI兼容(本地/自訂), 2 = Ollama
    };

    explicit TextPolishService(QNetworkAccessManager* networkManager, QObject *parent = nullptr);

    void polishText(const QString& inputText, const RequestParams &params);
    void testConnection(const RequestParams &params);
    void fetchModels(const RequestParams& params);

signals:
    void polishFinished(bool success, const QString &text, const QString &error);
    void connectionTested(bool success, const QString &message);
    void modelsFetched(bool success, const QStringList &models, const QString &error);

private:
    QNetworkAccessManager *m_manager;
};
