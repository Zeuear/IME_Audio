#include "GeminiProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "TextPolishPrompts.h"
#include "utils/Logger.h"

using namespace TextPolishPrompts;

QString GeminiProvider::buildEndpoint(const TextPolishService::RequestParams& params) {
    QString base = params.customUrl.isEmpty()
        ? QStringLiteral("https://generativelanguage.googleapis.com/v1beta/models/")
        : params.customUrl;

    QString model = params.model.isEmpty() ? QStringLiteral("gemini-1.5-flash") : params.model;
    return QString("%1%2:generateContent?key=%3").arg(base, model, params.apiKey);
}

QString GeminiProvider::buildPolishPrompt(const QString& inputText, const TextPolishService::RequestParams& params) {
    const PolishInstructionParts p = buildPolishInstructionParts(params);

    QString basePrompt = loadSystemPrompt("system_prompt_gemini.txt",
        "你是一个专业的语音输入优化助理。请对以下【原始转写文字】进行智能优化、重写与排版。\n"
        "【核心任务】:\n"
        "1. 移除无意义的口头语、填充词（如“嗯”、“呃”、“啊”、“然后”、“那个”、“你知道的”等）。\n"
        "2. 移除不必要的重复词汇与结巴。\n"
        "3. 自动识别并处理自我修正（例如“不对，应该上午十点” -> 直接改为“上午十点”）。\n"
        "4. 自动将口述的清单、步骤或要点整理成干净、结构化的分行列表或标点符号排版。\n"
        "【输出规则】:\n"
        "- 请“仅”输出优化后的最终文本，不要包含任何解释、Markdown代码块标记、导言或括号说明。\n"
        "- 如果原始文字自动识别为无意义的噪音、空字串或无法理清的语音，请直接输出空字串。");

    return QStringLiteral("%1\n\n【额外指示】:\n5. %2%3%4%5%6\n\n【原始转写文字】:\n\"%7\"")
        .arg(basePrompt, p.style, p.langInstruction, p.dictInstruction, p.vocabInstruction, p.customInstruction, inputText);
}

QString GeminiProvider::buildTranscribePromptText(const TextPolishService::RequestParams& params) {
    const PolishInstructionParts p = buildPolishInstructionParts(params);

    QString basePrompt = loadSystemPrompt("system_prompt_gemini_transcribe.txt",
        "请将附带的语音直接转录为文字，并同时进行智能优化、重写与排版。\n"
        "【核心任务】:\n"
        "1. 自动去除口头赘词与无意义填充词（如“嗯”、“呃”、“啊”、“然后”、“那个”、“你知道的”等）。\n"
        "2. 消除语音中的结巴与重复词汇。\n"
        "3. 自动识别并处理自我修正（只保留修正后的最终意图，不要保留口误）。\n"
        "4. 自动将口述清单、步骤或重点整理为干净分行列表或合适的标点符号排版。");

    return QStringLiteral("%1\n\n【额外指示】:\n5. %2%3%4%5%6").arg(basePrompt, p.style, p.langInstruction, p.dictInstruction, p.vocabInstruction, p.customInstruction);
}

QByteArray GeminiProvider::buildTextRequestBody(const QString& text, const TextPolishService::RequestParams& params) {
    QString prompt = buildPolishPrompt(text, params);
    QJsonObject textPart{ {"text", prompt} };
    QJsonArray partsArray{ textPart };
    QJsonObject content{ {"role", "user"},{"parts", partsArray} };
    QJsonArray contentsArray{ content };
    QJsonObject root{ {"contents", contentsArray}, { "generationConfig", QJsonObject{{"temperature", 0.1}} } };
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray GeminiProvider::buildAudioRequestBody(const QByteArray& audioBase64, const TextPolishService::RequestParams& params) {
    QJsonObject inlineData{ {"mimeType", "audio/wav"}, {"data", QString::fromLatin1(audioBase64)} };
    QJsonObject audioPart{ {"inlineData", inlineData} };

    QString prompt = buildTranscribePromptText(params);

    QJsonObject textPart{ {"text", prompt} };
    QJsonArray parts{ textPart, audioPart };
    QJsonObject content{ {"role", "user"}, {"parts", parts} };
    QJsonObject root{
        {"contents", QJsonArray{content}},
        {"generationConfig", QJsonObject{{"temperature", 0.1}}}
    };
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QString GeminiProvider::extractTextFromResponse(const QByteArray& responseJson, QString* error) {
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(responseJson, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        if (error) *error = QStringLiteral("响应解析失败: %1").arg(parseErr.errorString());
        return {};
    }
    QJsonObject root = doc.object();
    if (root.contains("error")) {
        if (error) *error = root["error"].toObject().value("message").toString();
        return {};
    }
    QJsonArray candidates = root.value("candidates").toArray();
    if (candidates.isEmpty()) {
        if (error) *error = QStringLiteral("无候选结果");
        return {};
    }
    QJsonArray parts = candidates.first().toObject().value("content").toObject().value("parts").toArray();
    QString result;
    for (const auto& p : parts)
        result += p.toObject().value("text").toString();
    return result.trimmed();
}

void GeminiProvider::polish(const QString& inputText,
                            const TextPolishService::RequestParams& params,
                            QNetworkAccessManager* mgr,
                            ResultCallback done) {
    if (params.apiKey.isEmpty()) {
        done(false, {}, QStringLiteral("没有配置 API Key"));
        return;
    }
    QNetworkRequest req(QUrl(buildEndpoint(params)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QByteArray postData = buildTextRequestBody(inputText, params);
    QNetworkReply* reply = mgr->post(req, postData);
    QObject::connect(reply, &QNetworkReply::finished, mgr, [reply, done]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done(false, {}, reply->errorString());
            return;
        }
        QString error;
        QString text = GeminiProvider::extractTextFromResponse(reply->readAll(), &error);
        done(error.isEmpty(), text, error);
    });
}

void GeminiProvider::transcribe(const QByteArray& wavBytes,
                                const TextPolishService::RequestParams& params,
                                QNetworkAccessManager* mgr,
                                ResultCallback done) {
    if (wavBytes.isEmpty()) {
        done(false, {}, QStringLiteral("音频数据为空"));
        return;
    }
    if (params.apiKey.isEmpty()) {
        done(false, {}, QStringLiteral("API密钥为空"));
        return;
    }

    QByteArray audioB64 = wavBytes.toBase64();
    QNetworkRequest req(QUrl(buildEndpoint(params)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = mgr->post(req, buildAudioRequestBody(audioB64, params));
    QObject::connect(reply, &QNetworkReply::finished, mgr, [reply, done]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done(false, {}, reply->errorString());
            return;
        }
        QString error;
        QString text = GeminiProvider::extractTextFromResponse(reply->readAll(), &error);
        done(error.isEmpty(), text, error);
    });
}

void GeminiProvider::test(const TextPolishService::RequestParams& params,
                          QNetworkAccessManager* mgr,
                          TestCallback done) {
    if (params.apiKey.isEmpty()) {
        done(false, QStringLiteral("没有配置 API Key"));
        return;
    }
    QNetworkRequest req(QUrl(buildEndpoint(params)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject textPart{ {"text", "ping"} };
    QJsonArray partsArray{ textPart };
    QJsonObject content{ {"parts", partsArray} };
    QJsonArray contentsArray{ content };
    QJsonObject root{ {"contents", contentsArray} };

    QByteArray postData = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = mgr->post(req, postData);
    QObject::connect(reply, &QNetworkReply::finished, mgr, [reply, done]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done(false, reply->errorString());
            return;
        }
        QString error;
        GeminiProvider::extractTextFromResponse(reply->readAll(), &error);
        if (!error.isEmpty()) {
            done(false, QStringLiteral("连接失败"));
        } else {
            done(true, QStringLiteral("连接成功"));
        }
    });
}



void GeminiProvider::fetchModels(const QString& baseUrl,
    QNetworkAccessManager* mgr,
    ModelsCallback done) {

    QStringList gemini_model_items;
    gemini_model_items << "gemini-3.1-flash-lite";
    gemini_model_items << "gemini-3-flash-preview";
    gemini_model_items << "gemini-flash-lite-latest";
    gemini_model_items << "gemini-2.5-flash";
    gemini_model_items << "gemini-2.5-pro";
    gemini_model_items << "gemini-1.5-flash";
    gemini_model_items << "gemini-1.5-pro";

    done(true, gemini_model_items, {});
}