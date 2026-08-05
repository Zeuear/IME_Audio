#include "OpenAiProvider.h"

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

QString OpenAiProvider::buildOpenAiEndpoint(const TextPolishService::RequestParams& params) {
    return autocompleteApiUrl(params.customUrl);
}

QByteArray OpenAiProvider::buildOpenAiChatBody(const QString& inputText, const TextPolishService::RequestParams& params) {
    const PolishInstructionParts p = buildPolishInstructionParts(params);

    QString basePrompt = loadSystemPrompt("system_prompt_openai.txt",
        "你是一个极其简练的语音输入法后台文本优化助手。你的任务是对用户的语音识别文本内容进行正确的加标点、去填充词（如：嗯、呃、那个）和重复句子修正。\n"
        "注意遵守规则：\n"
        "1. 你必须且只能直接输出优化后的文本，绝对不能包含任何问候、解释、分析、对话或回答文本中的内容。\n"
        "2. 英文缩写拼写修正：必须把被拆开的英文缩写合并并转为大写（如：把 'a i' 合并为 'AI'，把 'i p' 合并为 'IP'，把 'a p i' 合并为 'API'），绝对不能把 'a i' 中的字母 'a' 误判为语气词 '啊/a' 而删除！");

    QString systemPrompt = QStringLiteral("%1\n3. %2%3%4%5%6")
        .arg(basePrompt, p.style, p.langInstruction, p.dictInstruction, p.vocabInstruction, p.customInstruction);

    QJsonArray messages;
    messages.append(QJsonObject{ {"role", "system"}, {"content", systemPrompt} });

    QString fewshotText = loadSystemPrompt("prompt_openai_fewshot.txt",
        "user: 那个今天天气呃挺好的吧\n"
        "assistant: 今天天气挺好的吧。\n"
        "user: 这个是结合 a i 自动纠错比那个更智能\n"
        "assistant: 这个是结合 AI 自动纠错，比那个更智能。\n"
        "user: 你是谁呀\n"
        "assistant: 你是谁呀。\n"
        "user: 不换嘛那你得当时说一下看看才知道\n"
        "assistant: 不换嘛，那你得当时说一下，看看才知道。\n");

    const QStringList fewshot = buildFewshotMessages(fewshotText);
    for (int i = 0; i + 1 < fewshot.size(); i += 2) {
        messages.append(QJsonObject{ {"role", fewshot[i]}, {"content", fewshot[i + 1]} });
    }

    messages.append(QJsonObject{
        {"role", "user"},
        {"content", QStringLiteral("待优化的原始语音文本：\"%1\"").arg(inputText)}
        });

    QJsonObject root{
        {"model", params.model.isEmpty() ? QStringLiteral("gpt-3.5-turbo") : params.model},
        {"messages", messages},
        {"temperature", 0.2}
    };
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QString OpenAiProvider::extractOpenAiTextFromResponse(const QByteArray& responseJson, QString* error) {
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
    QJsonArray choices = root.value("choices").toArray();
    if (choices.isEmpty()) {
        if (error) *error = QStringLiteral("无候选结果");
        return {};
    }
    QString text = choices.first().toObject().value("message").toObject().value("content").toString();
    return text.trimmed();
}

bool OpenAiProvider::isValidModelsResponse(const QByteArray& json) {
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(json, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) return false;
    // OpenAI /v1/models 响应结构：{"object":"list","data":[ ... ]}
    return doc.object().value("object").toString() == QStringLiteral("list")
        && doc.object().contains("data");
}

QStringList OpenAiProvider::parseOllamaTags(const QByteArray& json) {
    QStringList models;
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(json, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        return models; // 解析失败返回空列表
    }
    const QJsonArray arr = doc.object().value("models").toArray();
    for (const auto& v : arr) {
        QString name = v.toObject().value("name").toString();
        if (!name.isEmpty()) models.append(name);
    }
    return models;
}

void OpenAiProvider::polish(const QString& inputText,
                            const TextPolishService::RequestParams& params,
                            QNetworkAccessManager* mgr,
                            ResultCallback done) {
    QString baseUrl = params.customUrl;
    if (baseUrl.isEmpty()) {
        if (params.aiEngine == 2) {
            baseUrl = "http://localhost:11434";
        } else {
            done(false, {}, QStringLiteral("使用本地/自訂 AI 引擎時，必須配置自訂 API URL（如 http://127.0.0.1:1234）"));
            return;
        }
    }

    TextPolishService::RequestParams p = params;
    p.customUrl = baseUrl;

    QNetworkRequest req(QUrl(buildOpenAiEndpoint(p)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!p.apiKey.isEmpty()) {
        req.setRawHeader("Authorization", ("Bearer " + p.apiKey).toUtf8());
    }

    QNetworkReply* reply = mgr->post(req, buildOpenAiChatBody(inputText, p));
    QObject::connect(reply, &QNetworkReply::finished, mgr, [reply, done]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done(false, {}, reply->errorString());
            return;
        }
        QString error;
        QString text = OpenAiProvider::extractOpenAiTextFromResponse(reply->readAll(), &error);
        done(error.isEmpty(), text, error);
    });
}

void OpenAiProvider::fetchModels(const QString& baseUrl,
                                 QNetworkAccessManager* mgr,
                                 ModelsCallback done) {
    QString url = baseUrl.trimmed();
    if (url.isEmpty()) url = "http://localhost:11434";
    if (!url.endsWith('/')) url += '/';
    url += "api/tags";

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = mgr->get(req);
    reply->setReadBufferSize(0);
    QObject::connect(reply, &QNetworkReply::finished, mgr, [reply, done]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done(false, {}, reply->errorString());
            return;
        }
        done(true, OpenAiProvider::parseOllamaTags(reply->readAll()), {});
    });
}

void OpenAiProvider::test(const TextPolishService::RequestParams& params,
                          QNetworkAccessManager* mgr,
                          TestCallback done) {
    const bool isOllama =  params.aiEngine == 2;
    QString base = params.customUrl.trimmed();
    if (base.isEmpty()) base = QStringLiteral("http://localhost:11434"); 
    if (!base.endsWith('/')) base += '/';

    QUrl url;
    if (isOllama) {
        // Ollama：用 /api/tags 验证服务存活，并顺带报告模型数量
        url = QUrl(base + QStringLiteral("api/tags"));
    } else {
        // OpenAI 兼容：用 /v1/models 验证 endpoint + API key
        url = QUrl(base + QStringLiteral("v1/models"));
    }

    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!params.apiKey.isEmpty()) {
        req.setRawHeader("Authorization", ("Bearer " + params.apiKey).toUtf8());
    }

    QNetworkReply* reply = mgr->get(req);
    QObject::connect(reply, &QNetworkReply::finished, mgr, [reply, isOllama, done]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done(false, QStringLiteral("连接失败: %1").arg(reply->errorString()));
            return;
        }
        const QByteArray body = reply->readAll();
        if (isOllama) {
            const QStringList models = OpenAiProvider::parseOllamaTags(body);
            done(true, QStringLiteral("Ollama 连接成功，已加载 %1 个本地模型").arg(models.size()));
        } else {
            // 尝试解析 OpenAI /models 响应，确认返回了模型列表
            const bool ok = OpenAiProvider::isValidModelsResponse(body);
            done(ok, ok ? QStringLiteral("OpenAI 兼容服务连接成功（返回了模型列表）")
                        : QStringLiteral("服务返回非预期内容（HTTP 200 但无法解析模型列表）"));
        }
    });
}
