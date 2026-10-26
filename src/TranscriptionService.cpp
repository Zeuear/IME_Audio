#include "TranscriptionService.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>
#include <QTimer>
#include "textpolish/GeminiProvider.h"
#include "TermsLibraryManager.h"
#include "sherpa/LanguageCatalog.h"
#include "TranslationService.h"
#include "translation/TranslationStage.h"
#include "utils/EnhancementFallback.h"
#include "utils/Logger.h"

TranscriptionService::TranscriptionService(
    QNetworkAccessManager* networkManager, 
    SherpaManager* sherpaManager,
    TextPolishService* textPolishService,
    TranslationService* translationService,
    const AppConfig& config, QObject *parent)
    : ITranscription(parent),
      m_config(config),
      m_manager(networkManager),
      m_sherpaManager(sherpaManager),
      m_textPolishService(textPolishService),
      m_translationService(translationService),
      m_textProcessor(new TextPostProcessor(this))
{
    connect(m_textPolishService, &TextPolishService::connectionTested, this,
        [this](bool success, const QString& message) {
            if (!success) {
                LOG_ERROR("Connection test failed!");
            }
            else {
                LOG_INFO("Connection test succeeded!");
            }
        });

    connect(m_sherpaManager, &SherpaManager::utteranceTranscribed, this, &TranscriptionService::onSherpaTranscribed);
    // 卸载会清掉 Sherpa 的任务队列，排着的请求不会再有结果，按失败放行
    connect(m_sherpaManager, &SherpaManager::idleUnloaded, this, [this]() {
        while (!m_sherpaTickets.isEmpty()) finish(m_sherpaTickets.dequeue(), false, {}, QStringLiteral("模型已卸载"));
    });
}

void TranscriptionService::transcribe(const QByteArray& pcmData, int sampleRate, int channels, int bitsPerSample){
    const Ticket t{ m_results.reserve(), m_fileMode };
    // 每个序号都必须有结果，否则会挡住后面所有结果；SherpaManager 对空音频不入队也不回结果
    if (pcmData.isEmpty()) {
        finish(t, false, {}, QStringLiteral("音频为空"));
        return;
    }

    if (m_config.backend == AsrBackendKind::Sherpa) {
        m_sherpaTickets.enqueue(t);
        m_sherpaManager->transcribeAsync(pcmData, sampleRate);
        return;
    }

    armTimeout(t);
    QByteArray wavBytes = buildWavBytes(pcmData, sampleRate, channels, bitsPerSample);
    switch (m_config.backend) {
    case AsrBackendKind::Groq:   transcribeGroq(wavBytes, t); break;
    case AsrBackendKind::Gladia: transcribeGladia(wavBytes, t); break;
    case AsrBackendKind::Gemini: transcribeGemini(wavBytes, t); break;
    default: finish(t, false, {}, QStringLiteral("未知的识别后端")); break;
    }
}

void TranscriptionService::onSherpaTranscribed(bool ok, const QString& text, const QString& err) {
    // SherpaManager 只在空闲卸载时清队列，而录音 / 文件转录期间不会空闲卸载，所以这里不该为空
    if (m_sherpaTickets.isEmpty()) {
        LOG_WARN("Sherpa result without a pending request, dropped");
        return;
    }
    const Ticket t = m_sherpaTickets.dequeue();
    if (!ok || !m_config.polish.enableAiEnhancement || m_config.polish.usesLocalTranslation()) {
        finish(t, ok, text, err);
        return;
    }

    TextPolishService::RequestParams params;
    params.aiEngine = m_config.polish.aiEngineIndex;
    params.apiKey = m_config.polish.apiKey;
    params.customUrl = m_config.polish.apiUrl;
    params.model = m_config.polish.model;
    params.style = m_config.polish.aiStyle;
    params.customPrompt = m_config.polish.prompt;
    params.aiVocab = m_config.polish.vocab;
    params.targetLang = m_config.polish.cloudTargetLang();

    // 记录润色前文字：润色请求超时时用它回退，避免整句被丢弃
    m_polishOriginals.insert(t.seq, text);
    armTimeout(t);
    m_textPolishService->polishText(text, params, [this, t, text](bool success, const QString& polished, const QString& error) {
        // 超时已经按原文完成过，迟到的真实结果直接忽略，避免重复提示
        if (m_polishOriginals.remove(t.seq) == 0) return;

        if (success) {
            finish(t, true, polished, {});
            return;
        }

        LOG_WARN(QString("Text polish failed: %1").arg(error));
        const auto outcome = decideEnhancementOutcome(false, text, polished);
        if (outcome.usedFallback) emit enhancementFailed(error, t.fileMode);
        finish(t, outcome.success, outcome.text, outcome.success ? QString() : error);
    });
}

void TranscriptionService::transcribeGemini(const QByteArray &wavBytes, const Ticket& t) {
    TextPolishService::RequestParams p;
    p.apiKey = m_config.polish.apiKey;
    p.model = m_config.polish.model;
    p.customUrl = m_config.polish.apiUrl;
    p.targetLang = m_config.polish.cloudTargetLang();
    p.style = m_config.polish.aiStyle;
    p.customPrompt = m_config.polish.prompt;
    p.replaceRules = m_config.replaceRules;
    p.aiVocab = m_config.polish.vocab;
    p.aiEngine = m_config.polish.aiEngineIndex;

    GeminiProvider provider;
    provider.transcribe(wavBytes, p, m_manager, [this, t](bool ok, QString text, QString err) {
        finish(t, ok, text, err);
    });
}

void TranscriptionService::transcribeGroq(const QByteArray &wavBytes, const Ticket& t) {
    // Groq Whisper API：multipart/form-data 上传音频文件
    if (m_config.groqKey.isEmpty()) {
        finish(t, false, {}, QStringLiteral("Groq API Key 未配置"));
        return;
    }

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    auto* buffer = new QBuffer(multiPart);
    buffer->setData(wavBytes);
    buffer->open(QIODevice::ReadOnly);


    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant(QStringLiteral("form-data; name=\"file\"; filename=\"audio.wav\"")));
    filePart.setBodyDevice(buffer);
    multiPart->append(filePart);

    QHttpPart modelPart;
    modelPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                         QVariant(QStringLiteral("form-data; name=\"model\"")));
    modelPart.setBody("whisper-large-v3");
    multiPart->append(modelPart);

    QNetworkRequest req(QUrl("https://api.groq.com/openai/v1/audio/transcriptions"));
    req.setRawHeader("Authorization", ("Bearer " + m_config.groqKey).toUtf8());

    QNetworkReply *reply = m_manager->post(req, multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, t]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finish(t, false, {}, reply->errorString());
            return;
        }
        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QString text = obj.value("text").toString();
        finish(t, true, text, {});
    });
}

void TranscriptionService::transcribeGladia(const QByteArray &wavBytes, const Ticket& t) {
    // Gladia v2 真实流程：上传拿 audio_url → 提交转录（异步）→ 轮询 result_url
    if (m_config.gladiaKey.isEmpty()) {
        finish(t, false, {}, QStringLiteral("Gladia API Key 未配置"));
        return;
    }

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    auto* buffer = new QBuffer(multiPart);
    buffer->setData(wavBytes);
    buffer->open(QIODevice::ReadOnly);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant(QStringLiteral("form-data; name=\"audio\"; filename=\"audio.wav\"")));
    filePart.setBodyDevice(buffer);
    multiPart->append(filePart);

    QNetworkRequest upReq(QUrl("https://api.gladia.io/v2/upload"));
    upReq.setRawHeader("x-gladia-key", m_config.gladiaKey.toUtf8());

    QNetworkReply *upReply = m_manager->post(upReq, multiPart);
    multiPart->setParent(upReply);

    connect(upReply, &QNetworkReply::finished, this, [this, upReply, t]() {
        upReply->deleteLater();
        if (upReply->error() != QNetworkReply::NoError) {
            finish(t, false, {}, upReply->errorString());
            return;
        }
        QJsonObject upObj = QJsonDocument::fromJson(upReply->readAll()).object();
        QString audioUrl = upObj.value("audio_url").toString();
        if (audioUrl.isEmpty()) {
            finish(t, false, {}, QStringLiteral("Gladia 上传失败：未返回 audio_url"));
            return;
        }
        submitGladiaTranscription(audioUrl, t);
    });
}

void TranscriptionService::submitGladiaTranscription(const QString &audioUrl, const Ticket& t) {
    QJsonObject body;
    body["audio_url"] = audioUrl;
    QJsonDocument doc(body);

    QNetworkRequest req(QUrl("https://api.gladia.io/v2/transcription"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-gladia-key", m_config.gladiaKey.toUtf8());

    QNetworkReply *reply = m_manager->post(req, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, t]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finish(t, false, {}, reply->errorString());
            return;
        }
        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QString resultUrl = obj.value("result_url").toString();
        if (resultUrl.isEmpty()) {
            finish(t, false, {}, QStringLiteral("Gladia 转录提交失败：未返回 result_url"));
            return;
        }
        pollGladiaResult(resultUrl, 0, t);
    });
}

void TranscriptionService::pollGladiaResult(const QString &resultUrl, int attempt, const Ticket& t) {
    static const int kMaxAttempts = 60; // ~60s @ 1s
    if (attempt >= kMaxAttempts) {
        finish(t, false, {}, QStringLiteral("Gladia 转录超时"));
        return;
    }

    QNetworkRequest req{ QUrl(resultUrl) };
    req.setRawHeader("x-gladia-key", m_config.gladiaKey.toUtf8());

    QNetworkReply *reply = m_manager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, resultUrl, attempt, t]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finish(t, false, {}, reply->errorString());
            return;
        }
        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QString status = obj.value("status").toString();
        if (status == QStringLiteral("done")) {
            QString text = obj.value("result").toObject()
                                .value("transcription").toObject()
                                .value("full_transcript").toString();
            finish(t, true, text, {});
            return;
        }
        if (status == QStringLiteral("error")) {
            finish(t, false, {}, QStringLiteral("Gladia 转录失败"));
            return;
        }
        // 仍处理中：1s 后重试
        QTimer::singleShot(1000, this, [this, resultUrl, attempt, t]() {
            pollGladiaResult(resultUrl, attempt + 1, t);
        });
    });
}

void TranscriptionService::finish(const Ticket& t, bool success, const QString& rawText, const QString& errorMsg) {
    // 同一序号只处理一次；翻译中的超时交给 TranslationService 自己的超时
    if (!m_results.isPending(t.seq) || m_translating.contains(t.seq)) return;

    if (!success) {
        completeResult(t, false, rawText, {}, errorMsg);
        return;
    }

    // 翻译必须发生在 postProcess 之后、进入有序放行之前
    const QString processed = postProcess(rawText, t.fileMode);
    const auto stage = decideTranslationStage(t.fileMode, m_config.polish.usesLocalTranslation(),
                                              m_config.polish.translateSourceLang,
                                              m_config.polish.activeTargetLang(),
                                              LanguageCatalog::codeOf(m_config.sherpa.languageModel));
    if (!stage.shouldTranslate || processed.isEmpty() || !m_translationService || !m_translationService->hasBackend()) {
        completeResult(t, true, rawText, processed, {});
        return;
    }

    m_translating.insert(t.seq);
    m_translationService->translate(processed, stage.from, stage.to,
        [this, t, rawText, processed](bool ok, const QString& translated, const QString& error) {
            m_translating.remove(t.seq);
            const auto outcome = decideTranslationOutcome(ok, translated, processed);
            if (outcome.usedFallback) {
                LOG_WARN(QString("Translation failed, fallback to original: %1").arg(error));
                emit translationFailed(error);
            }
            completeResult(t, true, rawText, outcome.text, {});
        });
}

void TranscriptionService::completeResult(const Ticket& t, bool success, const QString& rawText,
                                           const QString& finalText, const QString& errorMsg) {
    const Result r{ success, rawText, finalText, errorMsg, t.fileMode };
    if (!m_results.complete(t.seq, r)) return;

    for (const Result& ready : m_results.takeReady()) {
        if (ready.fileMode)
            emit fileSegmentFinished(ready.success, ready.rawText, ready.finalText, ready.errorMsg);
        else
            emit transcriptionFinished(ready.success, ready.rawText, ready.finalText, ready.errorMsg);
    }
}

void TranscriptionService::armTimeout(const Ticket& t) {
    QTimer::singleShot(kNetworkTimeoutMs, this, [this, t]() {
        // 翻译阶段由 TranslationService 的独立超时兜底，别被这个更长的识别超时抢先判失败
        if (m_translating.contains(t.seq)) return;

        // 润色阶段超时：识别原文已经拿到，回退原文继续，不能当成识别失败
        auto it = m_polishOriginals.find(t.seq);
        if (it != m_polishOriginals.end()) {
            const QString original = it.value();
            m_polishOriginals.erase(it);

            const QString cause = QStringLiteral("请求超时，无响应");
            LOG_WARN(QString("Text polish timeout, fallback to original: %1").arg(cause));
            const auto outcome = decideEnhancementOutcome(false, original, {});
            if (outcome.usedFallback) {
                emit enhancementFailed(cause, t.fileMode);
                finish(t, true, outcome.text, {});
            }
            else {
                finish(t, false, {}, cause);
            }
            return;
        }
        finish(t, false, {}, QStringLiteral("请求超时，无响应"));
    });
}

void TranscriptionService::testKey(AsrBackendKind backend, const QString& key, KeyTestCallback done) {
    if (key.isEmpty()) {
        done(false, QStringLiteral("Key 为空"));
        return;
    }
    // 各家都用"列出模型 / 列出任务"这类只读接口，Key 无效时返回 401/403
    QNetworkRequest req;
    switch (backend) {
    case AsrBackendKind::Groq:
        req.setUrl(QUrl("https://api.groq.com/openai/v1/models"));
        req.setRawHeader("Authorization", ("Bearer " + key).toUtf8());
        break;
    case AsrBackendKind::Gladia:
        req.setUrl(QUrl("https://api.gladia.io/v2/transcription?limit=1"));
        req.setRawHeader("x-gladia-key", key.toUtf8());
        break;
    case AsrBackendKind::Gemini:
        req.setUrl(QUrl("https://generativelanguage.googleapis.com/v1beta/models?pageSize=1"));
        req.setRawHeader("x-goog-api-key", key.toUtf8());
        break;
    default:
        done(false, QStringLiteral("该后端不需要 Key"));
        return;
    }
    req.setTransferTimeout(15000);

    QNetworkReply* reply = m_manager->get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError) {
            done(true, {});
        } else if (status == 400 || status == 401 || status == 403) {
            done(false, QStringLiteral("Key 无效或没有权限（HTTP %1）").arg(status));
        } else {
            LOG_WARN(QString("Key test failed: HTTP %1, %2").arg(status).arg(reply->errorString()));
            done(false, QStringLiteral("无法连接服务：%1").arg(reply->errorString()));
        }
    });
}

QString TranscriptionService::postProcess(const QString &rawText, bool fileMode) {

    m_textProcessor->setReplaceRules(m_config.replaceRules);
    m_textProcessor->setAutoPunctuation(!fileMode);
    return m_textProcessor->postProcess(rawText);
}

QByteArray TranscriptionService::buildWavBytes(const QByteArray& pcmData, int sampleRate, int channels, int bitsPerSample)
{
    QByteArray wav;
    QBuffer buffer(&wav);
    buffer.open(QIODevice::WriteOnly);

    QDataStream out(&buffer);
    out.setByteOrder(QDataStream::LittleEndian);

    quint32 dataSize = static_cast<quint32>(pcmData.size());
    quint32 byteRate = sampleRate * channels * bitsPerSample / 8;
    quint16 blockAlign = channels * bitsPerSample / 8;

    buffer.write("RIFF");
    out << quint32(36 + dataSize);
    buffer.write("WAVE");
    buffer.write("fmt ");
    out << quint32(16) << quint16(1) << quint16(channels)
        << quint32(sampleRate) << byteRate << blockAlign << quint16(bitsPerSample);
    buffer.write("data");
    out << dataSize;

    buffer.write(pcmData); 
    buffer.close();
    return wav;
}

