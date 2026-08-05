#include "TextPolishService.h"
#include "textpolish/TextPolishProvider.h"
#include "textpolish/GeminiProvider.h"
#include "textpolish/OpenAiProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "utils/Logger.h"

TextPolishService::TextPolishService(QNetworkAccessManager* networkManager, QObject* parent)
    : QObject(parent), m_manager(networkManager) {
}

void TextPolishService::polishText(const QString& inputText, const RequestParams& params) {
    auto provider = createTextPolishProvider(params);
    provider->polish(inputText, params, m_manager, [this](bool success, QString text, QString error) {
        emit polishFinished(success, text, error);
    });
}

void TextPolishService::testConnection(const RequestParams& params) {
    auto provider = createTextPolishProvider(params);
    if (!provider->canTest()) {
        emit connectionTested(false, QStringLiteral("该后端不支持连接测试"));
        return;
    }
    provider->test(params, m_manager, [this](bool success, QString message) {
        emit connectionTested(success, message);
    });
}

void TextPolishService::fetchModels(const RequestParams& params) {
    auto provider = createTextPolishProvider(params);
    if (!provider->canFetchModels()) {
        emit modelsFetched(false, {}, QStringLiteral("后端不支持拉取模型列表"));
        return;
    }
    provider->fetchModels(params.customUrl, m_manager, [this](bool success, QStringList models, QString error) {
        emit modelsFetched(success, models, error);
    });
}
