#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <functional>
#include <memory>
#include <utility>

class TextPolishProvider;

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

    // 引擎能力：UI 据此灰显控件，服务据此决定是否真的调用引擎
    struct Capabilities {
        bool canPolish = false;
        bool canTranslate = false;
        bool canTest = false;
        bool canFetchModels = false;
    };

    using PolishCallback = std::function<void(bool success, const QString& text, const QString& error)>;
    using ProviderFactory = std::function<std::unique_ptr<TextPolishProvider>(const RequestParams&)>;

    explicit TextPolishService(QNetworkAccessManager* networkManager, QObject *parent = nullptr);

    // 结果走回调而不是信号：并发润色时，每个调用方只收到自己那一句
    void polishText(const QString& inputText, const RequestParams &params, PolishCallback done);
    void testConnection(const RequestParams &params);
    void fetchModels(const RequestParams& params);

    // 供 UI 查询当前引擎能力；内部按 aiEngine 创建一次 provider 读取标志
    Capabilities capabilities(int aiEngine) const;

    // 仅测试使用：注入假 provider，生产代码路径不使用
    void setProviderFactoryForTest(ProviderFactory f) { m_providerFactory = std::move(f); }

signals:
    void connectionTested(bool success, const QString &message);
    void modelsFetched(bool success, const QStringList &models, const QString &error);

private:
    std::unique_ptr<TextPolishProvider> createProvider(const RequestParams& params) const;

    QNetworkAccessManager *m_manager;
    ProviderFactory m_providerFactory;
};
