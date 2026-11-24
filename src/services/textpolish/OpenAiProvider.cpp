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

namespace {
// Ollama 默认 5 分钟不用就卸载模型，听写间隔稍长就要重新加载几秒；常驻久一点
const QString kOllamaKeepAlive = QStringLiteral("30m");

double temperatureFor(const TextPolishService::RequestParams& params) {
    // 翻译要求确定性输出（翻译模型卡也要求 0）；润色保留一点改写余地
    return params.translateOnly ? 0.0 : 0.2;
}
} // namespace

QString OpenAiProvider::buildOpenAiEndpoint(const TextPolishService::RequestParams& params) {
    return autocompleteApiUrl(params.customUrl);
}

QString OpenAiProvider::ollamaBaseUrl(const QString& customUrl) {
    QString url = customUrl.trimmed();
    if (url.isEmpty()) url = QStringLiteral("http://localhost:11434");
    while (url.endsWith('/')) url.chop(1);
    // Ollama 本机只提供 http：误填成 https://localhost 时连接必然失败，按 http 处理
    for (const QString& host : { QStringLiteral("localhost"), QStringLiteral("127.0.0.1") }) {
        if (url.startsWith(QStringLiteral("https://") + host, Qt::CaseInsensitive))
            url = QStringLiteral("http://") + url.mid(QStringLiteral("https://").size());
    }
    for (const QString& suffix : { QStringLiteral("/v1/chat/completions"), QStringLiteral("/api/chat"),
                                   QStringLiteral("/api"), QStringLiteral("/v1") }) {
        if (url.endsWith(suffix, Qt::CaseInsensitive)) {
            url.chop(suffix.size());
            break;
        }
    }
    return url + '/';
}

QJsonArray OpenAiProvider::buildMessages(const QString& inputText, const TextPolishService::RequestParams& params) {
    if (params.translateOnly) {
        // 翻译专用模型按训练时的格式只收一条 user 消息；加 system 规则反而容易跑题、附带解释
        if (isTranslationModel(params.model)) {
            return QJsonArray{
                QJsonObject{ {"role", "user"}, {"content", translationModelPrompt(params.targetLang, inputText)} },
            };
        }
        // 通用模型只翻译：不带润色的 fewshot，否则模型会照着示例去改写而不是翻译
        return QJsonArray{
            QJsonObject{ {"role", "system"}, {"content", translateOnlyPrompt(params.targetLang)} },
            QJsonObject{ {"role", "user"}, {"content", inputText} },
        };
    }

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
    return messages;
}

QByteArray OpenAiProvider::buildOpenAiChatBody(const QString& inputText, const TextPolishService::RequestParams& params) {
    QJsonObject root{
        {"model", params.model.isEmpty() ? QStringLiteral("gpt-3.5-turbo") : params.model},
        {"messages", buildMessages(inputText, params)},
        {"temperature", temperatureFor(params)}
    };
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray OpenAiProvider::buildOllamaChatBody(const QString& inputText, const TextPolishService::RequestParams& params,
                                               bool withThink) {
    QJsonObject root{
        {"model", params.model},
        {"messages", buildMessages(inputText, params)},
        {"stream", false},
        {"keep_alive", kOllamaKeepAlive},
        {"options", QJsonObject{ {"temperature", temperatureFor(params)} }},
    };
    // 思考型模型（Qwen3、Index-Translate 等）不关思考会先输出一大段推理，听写延迟翻倍
    if (withThink) root.insert("think", false);
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
    return stripThinking(text);
}

QString OpenAiProvider::extractOllamaChatText(const QByteArray& responseJson, QString* error) {
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(responseJson, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        if (error) *error = QStringLiteral("响应解析失败: %1").arg(parseErr.errorString());
        return {};
    }
    const QJsonObject root = doc.object();
    // Ollama 的错误是 {"error":"..."} 字符串，不是 OpenAI 的对象
    if (root.contains("error")) {
        if (error) *error = root.value("error").toString();
        return {};
    }
    if (!root.contains("message")) {
        if (error) *error = QStringLiteral("无候选结果");
        return {};
    }
    // thinking 字段单独返回，content 里只该有答案；仍过滤一遍，兼容把思考写进正文的模型
    return stripThinking(root.value("message").toObject().value("content").toString());
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

QStringList OpenAiProvider::parseOpenAiModels(const QByteArray& json) {
    QStringList models;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    for (const auto& v : doc.object().value("data").toArray()) {
        const QString id = v.toObject().value("id").toString();
        if (!id.isEmpty()) models.append(id);
    }
    return models;
}

void OpenAiProvider::polish(const QString& inputText,
                            const TextPolishService::RequestParams& params,
                            QNetworkAccessManager* mgr,
                            ResultCallback done) {
    if (params.aiEngine == 2) {
        if (params.model.trimmed().isEmpty()) {
            done(false, {}, QStringLiteral("还没有选择 Ollama 模型，请先在 AI 设置里下载或选择模型"));
            return;
        }
        polishOllama(inputText, params, mgr, std::move(done), true);
        return;
    }

    if (params.customUrl.isEmpty()) {
        done(false, {}, QStringLiteral("使用 OpenAI 兼容引擎时，必须填写 API 地址（如 http://127.0.0.1:1234）"));
        return;
    }

    QNetworkRequest req(QUrl(buildOpenAiEndpoint(params)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!params.apiKey.isEmpty()) {
        req.setRawHeader("Authorization", ("Bearer " + params.apiKey).toUtf8());
    }

    QNetworkReply* reply = mgr->post(req, buildOpenAiChatBody(inputText, params));
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

void OpenAiProvider::polishOllama(const QString& inputText, const TextPolishService::RequestParams& params,
                                  QNetworkAccessManager* mgr, ResultCallback done, bool withThink) {
    QNetworkRequest req(QUrl(ollamaBaseUrl(params.customUrl) + QStringLiteral("api/chat")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = mgr->post(req, buildOllamaChatBody(inputText, params, withThink));
    QObject::connect(reply, &QNetworkReply::finished, mgr,
        [reply, inputText, params, mgr, done, withThink]() mutable {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        QString error;
        const QString text = OpenAiProvider::extractOllamaChatText(body, &error);
        if (reply->error() == QNetworkReply::NoError && error.isEmpty()) {
            done(true, text, {});
            return;
        }
        // 旧版 Ollama 不认识 think 字段、或模型不支持关思考时报错，去掉该字段重试一次
        if (withThink && error.contains(QStringLiteral("think"), Qt::CaseInsensitive)) {
            LOG_WARN(QString("Ollama rejected think=false, retrying without it: %1").arg(error));
            polishOllama(inputText, params, mgr, std::move(done), false);
            return;
        }
        if (error.isEmpty()) error = reply->errorString();
        // 模型没下载时 Ollama 返回 404 + "model ... not found"，给出能操作的提示
        if (error.contains(QStringLiteral("not found"), Qt::CaseInsensitive))
            error = QStringLiteral("Ollama 里没有模型「%1」，请先在 AI 设置里下载").arg(params.model);
        done(false, {}, error);
    });
}

void OpenAiProvider::fetchModels(const QString& baseUrl,
                                 QNetworkAccessManager* mgr,
                                 ModelsCallback done) {
    const bool ollama = m_engine == 2;
    QString url;
    if (ollama) {
        url = ollamaBaseUrl(baseUrl) + QStringLiteral("api/tags");
    } else {
        // OpenAI 兼容服务（LM Studio、vLLM 等）没有 /api/tags，模型列表在 /v1/models
        QString base = baseUrl.trimmed();
        if (base.isEmpty()) {
            done(false, {}, QStringLiteral("请先填写 API 地址"));
            return;
        }
        while (base.endsWith('/')) base.chop(1);
        if (base.endsWith(QStringLiteral("/chat/completions"))) base.chop(QStringLiteral("/chat/completions").size());
        url = base.endsWith(QStringLiteral("/v1")) ? base + QStringLiteral("/models") : base + QStringLiteral("/v1/models");
    }

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!ollama && !m_apiKey.isEmpty()) req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    QNetworkReply* reply = mgr->get(req);
    reply->setReadBufferSize(0);
    QObject::connect(reply, &QNetworkReply::finished, mgr, [reply, done, ollama]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done(false, {}, reply->errorString());
            return;
        }
        const QByteArray body = reply->readAll();
        done(true, ollama ? OpenAiProvider::parseOllamaTags(body) : OpenAiProvider::parseOpenAiModels(body), {});
    });
}

void OpenAiProvider::test(const TextPolishService::RequestParams& params,
                          QNetworkAccessManager* mgr,
                          TestCallback done) {
    const bool isOllama =  params.aiEngine == 2;
    QUrl url;
    if (isOllama) {
        // Ollama：用 /api/tags 验证服务存活，并顺带报告模型数量
        url = QUrl(ollamaBaseUrl(params.customUrl) + QStringLiteral("api/tags"));
    } else {
        QString base = params.customUrl.trimmed();
        if (base.isEmpty()) base = QStringLiteral("http://localhost:11434");
        if (!base.endsWith('/')) base += '/';
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
            done(false, isOllama ? QStringLiteral("连接 Ollama 失败：%1（请确认已安装并启动 Ollama）").arg(reply->errorString())
                                 : QStringLiteral("连接失败: %1").arg(reply->errorString()));
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
