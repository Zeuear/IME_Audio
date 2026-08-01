#include "GeminiClient.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QUrl>
#include <QUrlQuery>
#include "utils/Logger.h"

GeminiClient::GeminiClient(QNetworkAccessManager* networkManager, QObject* parent)
    : QObject(parent), m_manager(networkManager) {
}

void GeminiClient::transcribeAudio(const QByteArray& wavBytes, const RequestParams& params) {
    if (wavBytes.isEmpty()) {
        emit transcribeFinished(false, {}, QStringLiteral("音频数据为空"));
        return;
    }
	if (params.apiKey.isEmpty()) {
		emit transcribeFinished(false, {}, QStringLiteral("API密钥为空"));
		return;
	}

    QByteArray audioB64 = wavBytes.toBase64();
    QNetworkRequest req(QUrl(buildEndpoint(params, true)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_manager->post(req, buildAudioRequestBody(audioB64, params));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit transcribeFinished(false, {}, reply->errorString());
            return;
        }
        QString error;
        QString text = extractTextFromResponse(reply->readAll(), &error);
        emit transcribeFinished(error.isEmpty(), text, error);
        });
}

void GeminiClient::ensurePromptsDirInited() {
    QString dir = QCoreApplication::applicationDirPath() + "/prompts";
    QDir().mkpath(dir);
}

QString GeminiClient::loadSystemPrompt(const QString& filename, const QString& defaultPrompt) const {
    const_cast<GeminiClient*>(this)->ensurePromptsDirInited();
    QString path = QCoreApplication::applicationDirPath() + "/prompts/" + filename;

    QFile file(path);
    if (!file.exists()) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(defaultPrompt.toUtf8());
            file.close();
        }
        return defaultPrompt;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return defaultPrompt;
    }

    QByteArray raw = file.readAll();
    file.close();

    if (raw.isEmpty()) {
        return defaultPrompt;
    }
    if (raw.size() >= 3 &&
        (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) {
        raw = raw.mid(3);
    }

    QString content = QString::fromUtf8(raw);
    return content.isEmpty() ? defaultPrompt : content;
}

QString GeminiClient::styleInstruction(const QString& style) const {
    if (style == QStringLiteral("商務正式")) {
        return loadSystemPrompt("prompt_style_business.txt",
            "请将文本改写得更加商务、正式、专业，适合职场与商务邮件沟通。");
    }
    else if (style == QStringLiteral("日常口語")) {
        return loadSystemPrompt("prompt_style_casual.txt",
            "请保持日常口语风格，使语句流畅自然，不要过于死板。");
    }
    else if (style == QStringLiteral("簡潔扼要")) {
        return loadSystemPrompt("prompt_style_concise.txt",
            "请尽可能简洁明了，去掉赘字，保留核心重点。");
    }
    return loadSystemPrompt("prompt_style_default.txt", "请优化语句，使其流畅、重点清晰。");
}

QStringList GeminiClient::buildFewshotMessages(const QString& fewshotText) const {
    QStringList result;
    if (fewshotText.isEmpty()) return result;

    const auto lines = fewshotText.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        QString role, content;

        if (line.startsWith("user:", Qt::CaseInsensitive)) {
            role = "user";
            content = line.mid(5).trimmed();
        }
        else if (line.startsWith(QStringLiteral("user\uFF1A"))) { // 全角冒号 "："
            role = "user";
            content = line.mid(QStringLiteral("user\uFF1A").length()).trimmed();
        }
        else if (line.startsWith("assistant:", Qt::CaseInsensitive)) {
            role = "assistant";
            content = line.mid(10).trimmed();
        }
        else if (line.startsWith(QStringLiteral("assistant\uFF1A"))) {
            role = "assistant";
            content = line.mid(QStringLiteral("assistant\uFF1A").length()).trimmed();
        }
        else {
            continue; 
        }

        if (!content.isEmpty()) {
            result << role << content;
        }
    }
    return result;
}

QString GeminiClient::buildPolishPrompt(const QString& inputText, const RequestParams& params) const {
    QString style = styleInstruction(params.style);

    QString langInstruction;
    if (params.targetLang != QStringLiteral("不翻譯") && !params.targetLang.isEmpty()) {
        langInstruction = QStringLiteral("并将最终结果翻译为「%1」（如果是中文请用对应的繁简体）。").arg(params.targetLang);
    }

    QString dictInstruction;
    if (!params.replaceRules.isEmpty()) {
        dictInstruction = QStringLiteral("\n【专有名词与字词拼写参考（若语音中听起来相近，请优先使用以下写法）】:\n%1\n").arg(params.replaceRules);
    }

    QString vocabInstruction;
    if (!params.aiVocab.isEmpty()) {
        vocabInstruction = QStringLiteral("\n【专有名词与自订词库（若原始文字中听起来相近，请优先使用且纠正为此处的写法）】:\n%1\n").arg(params.aiVocab);
    }

    QString customInstruction;
    if (!params.customPrompt.isEmpty() && params.style == QStringLiteral("自訂 Prompt")) {
        customInstruction = QStringLiteral("\n【自订额外要求】:\n%1\n").arg(params.customPrompt);
    }

    QString basePrompt = loadSystemPrompt("system_prompt_gemini.txt",
        "你是一个专业的语音输入优化助理。请对以下【原始转写文字】进行智能优化、重写与排版。\n"
        "【核心任务】:\n"
        "1. 移除无意义的口头语、填充词（如\u201c嗯\u201d、\u201c呃\u201d、\u201c啊\u201d、\u201c然后\u201d、\u201c那个\u201d、\u201c你知道的\u201d等）。\n"
        "2. 移除不必要的重复词汇与结巴。\n"
        "3. 自动识别并处理自我修正（例如\u201c不对，应该上午十点\u201d -> 直接改为\u201c上午十点\u201d）。\n"
        "4. 自动将口述的清单、步骤或要点整理成干净、结构化的分行列表或标点符号排版。\n"
        "【输出规则】:\n"
        "- 请\u201c仅\u201d输出优化后的最终文本，不要包含任何解释、Markdown代码块标记、导言或括号说明。\n"
        "- 如果原始文字自动识别为无意义的噪音、空字串或无法理清的语音，请直接输出空字串。");

    return QStringLiteral("%1\n\n【额外指示】:\n5. %2%3%4%5%6\n\n【原始转写文字】:\n\"%7\"")
        .arg(basePrompt, style, langInstruction, dictInstruction, vocabInstruction, customInstruction, inputText);
}

QString GeminiClient::buildTranscribePromptText(const RequestParams& params) const {
    QString style = styleInstruction(params.style);

    QString langInstruction;
    if (params.targetLang != QStringLiteral("不翻譯") && !params.targetLang.isEmpty()) {
        langInstruction = QStringLiteral("并将结果翻译为「%1」。").arg(params.targetLang);
    }

    QString dictInstruction;
    if (!params.replaceRules.isEmpty()) {
        dictInstruction = QStringLiteral("\n【专有名词与字词拼写参考（若语音中发音相近，请优先使用以下写法）】:\n%1\n").arg(params.replaceRules);
    }

    QString vocabInstruction;
    if (!params.aiVocab.isEmpty()) {
        vocabInstruction = QStringLiteral("\n【专有名词与自订词库（若原始文字中听起来相近，请优先使用且纠正为此处的写法）】:\n%1\n").arg(params.aiVocab);
    }

    QString customInstruction;
    if (!params.customPrompt.isEmpty() && params.style == QStringLiteral("自訂 Prompt")) {
        customInstruction = QStringLiteral("\n【自订额外要求】:\n%1\n").arg(params.customPrompt);
    }

    QString basePrompt = loadSystemPrompt("system_prompt_gemini_transcribe.txt",
        "请将附带的语音直接转录为文字，并同时进行智能优化、重写与排版。\n"
        "【核心任务】:\n"
        "1. 自动去除口头赘词与无意义填充词（如\u201c嗯\u201d、\u201c呃\u201d、\u201c啊\u201d、\u201c然后\u201d、\u201c那个\u201d、\u201c你知道的\u201d等）。\n"
        "2. 消除语音中的结巴与重复词汇。\n"
        "3. 自动识别并处理自我修正（只保留修正后的最终意图，不要保留口误）。\n"
        "4. 自动将口述清单、步骤或重点整理为干净分行列表或合适的标点符号排版。");

    return QStringLiteral("%1\n\n【额外指示】:\n5. %2%3%4%5%6\n\n"
        "【输出规则】:\n"
        "- 请\u201c仅\u201d输出优化后的转录文字，不要包含任何解释、Markdown代码块标记、导言说明。\n"
        "- 若音频中只有噪音，请输出空字串。")
        .arg(basePrompt, style, langInstruction, dictInstruction, vocabInstruction, customInstruction);
}

QString GeminiClient::autocompleteApiUrl(const QString& input) const {
    QString url = input;
    if (url.contains("/api/v1/chat")) return url;
    if (url.endsWith("/chat/completions", Qt::CaseInsensitive)) return url;

    if (!url.contains("/v1")) {
        url += url.endsWith('/') ? "v1/chat/completions" : "/v1/chat/completions";
    }
    else {
        url += url.endsWith('/') ? "chat/completions" : "/chat/completions";
    }
    return url;
}

QString GeminiClient::buildOpenAiEndpoint(const RequestParams& params) const {
    return autocompleteApiUrl(params.customUrl);
}

QString GeminiClient::buildEndpoint(const RequestParams& params, bool) const {
    QString base = params.customUrl.isEmpty()
        ? QStringLiteral("https://generativelanguage.googleapis.com/v1beta/models/")
        : params.customUrl;

    QString model = params.model.isEmpty() ? QStringLiteral("gemini-1.5-flash") : params.model;
    return QString("%1%2:generateContent?key=%3").arg(base, model, params.apiKey);
}

QByteArray GeminiClient::buildAudioRequestBody(const QByteArray& audioBase64, const RequestParams& params) const {
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

QByteArray GeminiClient::buildTextRequestBody(const QString& text, const RequestParams& params) const {
    QString prompt = buildPolishPrompt(text, params);

    QJsonObject textPart{ {"text", prompt} };
    QJsonArray partsArray{ textPart };
    QJsonObject content{ {"role", "user"},{"parts", partsArray} };
    QJsonArray contentsArray{ content };
    QJsonObject root{ {"contents", contentsArray}, { "generationConfig", QJsonObject{{"temperature", 0.1}} } };
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray GeminiClient::buildOpenAiChatBody(const QString& inputText, const RequestParams& params) const {
    QString style = styleInstruction(params.style);

    QString langInstruction;
    if (params.targetLang != QStringLiteral("不翻譯") && !params.targetLang.isEmpty()) {
        langInstruction = QStringLiteral("并将最终结果翻译为「%1」（如果是中文请用对应的繁简体）。").arg(params.targetLang);
    }

    QString dictInstruction;
    if (!params.replaceRules.isEmpty()) {
        dictInstruction = QStringLiteral("\n纠错词典（将谐音错字更正为）：\n%1\n").arg(params.replaceRules);
    }

    QString vocabInstruction;
    if (!params.aiVocab.isEmpty()) {
        vocabInstruction = QStringLiteral("\n【专有名词与自订词库（若原始文字中听起来相近，请优先使用且纠正为此处的写法）】:\n%1\n").arg(params.aiVocab);
    }

    QString customInstruction;
    if (!params.customPrompt.isEmpty() && params.style == QStringLiteral("自訂 Prompt")) {
        customInstruction = QStringLiteral("\n【自订额外要求】:\n%1\n").arg(params.customPrompt);
    }

    QString basePrompt = loadSystemPrompt("system_prompt_openai.txt",
        "你是一个极其简练的语音输入法后台文本优化助手。你的任务是对用户的语音识别文本内容进行正确的加标点、去填充词（如：嗯、呃、那个）和重复句子修正。\n"
        "注意遵守规则：\n"
        "1. 你必须且只能直接输出优化后的文本，绝对不能包含任何问候、解释、分析、对话或回答文本中的内容。\n"
        "2. 英文缩写拼写修正：必须把被拆开的英文缩写合并并转为大写（如：把 'a i' 合并为 'AI'，把 'i p' 合并为 'IP'，把 'a p i' 合并为 'API'），绝对不能把 'a i' 中的字母 'a' 误判为语气词 '啊/a' 而删除！");

    QString systemPrompt = QStringLiteral("%1\n3. %2%3%4%5%6")
        .arg(basePrompt, style, langInstruction, dictInstruction, vocabInstruction, customInstruction);

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

QString GeminiClient::extractTextFromResponse(const QByteArray& responseJson, QString* error) const {
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

QString GeminiClient::extractOpenAiTextFromResponse(const QByteArray& responseJson, QString* error) const {
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

void GeminiClient::polishText(const QString& inputText, const RequestParams& params) {
    if (params.aiEngine == 1) {
        if (params.customUrl.isEmpty()) {
            emit polishFinished(false, {}, QStringLiteral("使用本地/自訂 AI 引擎時，必須配置自訂 API URL（如 http://127.0.0.1:1234）"));
            return;
        }

        QNetworkRequest req((QUrl(buildOpenAiEndpoint(params))));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        if (!params.apiKey.isEmpty()) {
            req.setRawHeader("Authorization", ("Bearer " + params.apiKey).toUtf8());
        }

        QNetworkReply* reply = m_manager->post(req, buildOpenAiChatBody(inputText, params));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit polishFinished(false, {}, reply->errorString());
                return;
            }
            QString error;
            QString text = extractOpenAiTextFromResponse(reply->readAll(), &error);
            emit polishFinished(error.isEmpty(), text, error);
            });
        return;
    }

    if (params.apiKey.isEmpty()) {
        emit polishFinished(false, {}, QStringLiteral("没有配置 API Key"));
        return;
    }
    // Gemini 分支
    QNetworkRequest req(QUrl(buildEndpoint(params, true)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QByteArray postData = buildTextRequestBody(inputText, params);
    QNetworkReply* reply = m_manager->post(req, postData);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        LOG_DEBUG("finished lambda entered");
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN(reply->errorString());
            emit polishFinished(false, {}, reply->errorString());
            return;
        }
        QString error;
        QString text = extractTextFromResponse(reply->readAll(), &error);
        emit polishFinished(error.isEmpty(), text, error);
    });
}

void GeminiClient::testConnection(const RequestParams& params) {
    QNetworkRequest req(QUrl(buildEndpoint(params, true)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    if (params.apiKey.isEmpty()) {
        emit connectionTested(false, QStringLiteral("没有配置 API Key"));
        return;
    }

    QJsonObject textPart{ {"text", "ping"} };
    QJsonArray partsArray{ textPart };
    QJsonObject content{ {"parts", partsArray} };
    QJsonArray contentsArray{ content };
    QJsonObject root{ {"contents", contentsArray} };

    QByteArray postData = QJsonDocument(root).toJson(QJsonDocument::Compact);
    
    QNetworkReply* reply = m_manager->post(req, postData);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QString errorString = reply->errorString();
            LOG_DEBUG(errorString);
            emit connectionTested(false, errorString);
            return;
        }
        QString error;
        extractTextFromResponse(reply->readAll(), &error);

        if (!error.isEmpty()) {
            emit connectionTested(false, QStringLiteral("连接失败"));
        }
        else {
            emit connectionTested(true, QStringLiteral("连接成功"));
        }
    });
}