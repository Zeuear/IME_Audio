#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <functional>

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

    using PolishCallback = std::function<void(bool success, const QString& text, const QString& error)>;

    explicit TextPolishService(QNetworkAccessManager* networkManager, QObject *parent = nullptr);

    // 结果走回调而不是信号：并发润色时，每个调用方只收到自己那一句
    void polishText(const QString& inputText, const RequestParams &params, PolishCallback done);
    void testConnection(const RequestParams &params);
    void fetchModels(const RequestParams& params);

signals:
    void connectionTested(bool success, const QString &message);
    void modelsFetched(bool success, const QStringList &models, const QString &error);

private:
    QNetworkAccessManager *m_manager;
};
