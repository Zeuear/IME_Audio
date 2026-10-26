#include "TextPolishService.h"
#include "AppConfig.h"
#include "textpolish/TextPolishProvider.h"
#include "textpolish/TextPolishPrompts.h"
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

std::unique_ptr<TextPolishProvider> TextPolishService::createProvider(const RequestParams& params) const {
    if (m_providerFactory) return m_providerFactory(params);
    return createTextPolishProvider(params);
}

TextPolishService::Capabilities TextPolishService::capabilities(int aiEngine) const {
    // 本地离线翻译不是云端 provider：不润色、只翻译，没有连接测试和模型列表
    if (aiEngine == kLocalTranslationEngine) return { false, true, false, false };

    RequestParams params;
    params.aiEngine = aiEngine;
    const auto provider = createProvider(params);
    if (!provider) return {};
    return { provider->canPolish(), provider->canTranslate(),
             provider->canTest(), provider->canFetchModels() };
}

void TextPolishService::polishText(const QString& inputText, const RequestParams& params, PolishCallback done) {
    if (params.aiEngine == kLocalTranslationEngine) {
        done(true, inputText, {});
        return;
    }

    auto provider = createProvider(params);
    if (!provider) {
        done(false, {}, QStringLiteral("未找到可用的润色引擎"));
        return;
    }

    // 引擎不能润色、也没有翻译任务时，原文就是结果；同步回调，保持调用方的顺序语义
    const bool hasTranslation = TextPolishPrompts::hasTranslationTarget(params.targetLang);
    if (!provider->canPolish() && !(provider->canTranslate() && hasTranslation)) {
        done(true, inputText, {});
        return;
    }

    provider->polish(inputText, params, m_manager, [done = std::move(done)](bool success, QString text, QString error) {
        done(success, text, error);
    });
}

void TextPolishService::testConnection(const RequestParams& params) {
    auto provider = createProvider(params);
    if (!provider || !provider->canTest()) {
        emit connectionTested(false, QStringLiteral("该后端不支持连接测试"));
        return;
    }
    provider->test(params, m_manager, [this](bool success, QString message) {
        emit connectionTested(success, message);
    });
}

void TextPolishService::fetchModels(const RequestParams& params) {
    auto provider = createProvider(params);
    if (!provider || !provider->canFetchModels()) {
        emit modelsFetched(false, {}, QStringLiteral("后端不支持拉取模型列表"));
        return;
    }
    provider->fetchModels(params.customUrl, m_manager, [this](bool success, QStringList models, QString error) {
        emit modelsFetched(success, models, error);
    });
}
