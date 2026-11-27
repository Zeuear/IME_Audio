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
#include "audio/AudioNormalizer.h"
#include "sherpa/HotwordPrompt.h"
#include "textpolish/AiStagePlan.h"
#include "textpolish/GeminiProvider.h"
#include "textpolish/TextPolishPrompts.h"
#include "TermsLibraryManager.h"
#include "sherpa/LanguageCatalog.h"
#include "TranslationService.h"
#include "translation/TranslationStage.h"
#include "ollama/OllamaErrors.h"
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

void TranscriptionService::transcribe(const QByteArray& rawPcm, int sampleRate, int channels, int bitsPerSample){
    const Ticket t{ m_results.reserve(), m_fileMode };
    // 每个序号都必须有结果，否则会挡住后面所有结果；SherpaManager 对空音频不入队也不回结果
    if (rawPcm.isEmpty()) {
        finish(t, false, {}, QStringLiteral("音频为空"));
        return;
    }

    QByteArray pcmData = rawPcm;
    if (channels == 1 && bitsPerSample == 16) {
        double gain = 1.0;
        pcmData = AudioNormalizer::normalizePcm16(rawPcm, &gain);
        if (gain > 1.0) LOG_DEBUG(QString("Audio normalized, gain x%1").arg(gain, 0, 'f', 1));
    }

    if (m_config.backend == AsrBackendKind::Sherpa) {
        m_sherpaTickets.enqueue(t);
        // 文件转录没有实时字幕可参考，沿用全局热词
        std::optional<QString> hotwords;
        if (!t.fileMode && m_hotwordHint && m_hotwordSelector.entryCount() > 0 && m_sherpaManager->supportsUtteranceHotwords()) {
            const QString hint = m_hotwordHint();
            if (!hint.isEmpty()) {
                const QStringList picked = m_hotwordSelector.select(hint);
                hotwords = HotwordPrompt::build(picked);
                LOG_DEBUG(QString("Hotwords picked for utterance: %1").arg(hotwords->isEmpty() ? QStringLiteral("(none)") : *hotwords));
            }
        }
        m_sherpaManager->transcribeAsync(pcmData, sampleRate, hotwords);
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
    if (!ok) {
        finish(t, ok, text, err);
        return;
    }
    runAiStages(t, text);
}

TextPolishService::RequestParams TranscriptionService::aiRequestParams() const {
    const PolishConfig& pc = m_config.polish;
    TextPolishService::RequestParams params;
    params.aiEngine = pc.aiEngineIndex;
    params.apiKey = pc.apiKey;
    params.customUrl = pc.apiUrl;
    params.model = pc.model;
    params.style = pc.aiStyle;
    params.customPrompt = pc.prompt;
    params.aiVocab = pc.vocab;
    return params;
}

void TranscriptionService::runAiStages(const Ticket& t, const QString& text) {
    const PolishConfig& pc = m_config.polish;
    // 没配置好的引擎直接用原文，不当作失败：否则听写时每句都弹一次"润色失败"；本地翻译在 finish 里做
    if (text.isEmpty() || pc.usesLocalTranslation() || !pc.cloudEngineConfigured()) {
        finish(t, true, text, {});
        return;
    }
    // 文件转录保留原文，译文由 translateForFile 另出，这里不能顺带翻译
    const QString target = t.fileMode ? QStringLiteral("不翻译") : pc.cloudTargetLang();
    const auto caps = m_textPolishService->capabilities(pc.aiEngineIndex);
    const AiStagePlan plan = planAiStages(pc.enableAiEnhancement, target, caps.canPolish, caps.canTranslate,
                                          pc.model, pc.effectiveTranslateModel());
    if (!plan.polish && !plan.translate) {
        finish(t, true, text, {});
        return;
    }

    // 要翻译的句子：识别原文先给界面显示（此时还没经过润色 / 翻译），译文回来后补上
    if (plan.translate) showSourceFirst(t.seq, text);

    // 从这里开始这一句归 m_aiJobs 管：回调和超时兜底谁先 take 谁收尾
    m_aiJobs.begin(t.seq, text, plan, target);
    armTimeout(t);
    if (plan.polish) runAiPolish(t);
    else runAiTranslation(t);
}

void TranscriptionService::runAiPolish(const Ticket& t) {
    const AiJob* job = m_aiJobs.find(t.seq);
    if (!job) return;
    TextPolishService::RequestParams params = aiRequestParams();
    // 同一个通用模型时润色和翻译合成一次请求；否则润色只润色，翻译交给下一段
    params.targetLang = job->plan.combined ? job->target : QStringLiteral("不翻译");
    m_textPolishService->polishText(job->current, params,
        [this, t](bool ok, const QString& polished, const QString& error) { onAiPolishDone(t, ok, polished, error); });
}

void TranscriptionService::onAiPolishDone(const Ticket& t, bool ok, const QString& polished, const QString& error) {
    const AiJob* job = m_aiJobs.find(t.seq);
    if (!job) return;   // 超时已经按原文结束过，迟到的真实结果直接忽略，避免重复提示

    QString next = polished;
    if (!ok) {
        LOG_WARN(QString("Text polish failed: %1").arg(error));
        const auto outcome = decideEnhancementOutcome(false, job->source, polished);
        if (outcome.usedFallback) emit enhancementFailed(explainOllamaError(error), t.fileMode);
        if (!outcome.success) {
            m_aiJobs.take(t.seq);
            finish(t, false, {}, error);
            return;
        }
        next = outcome.text;
    }

    if (job->plan.translate && !job->plan.combined) {
        m_aiJobs.advance(t.seq, next);
        runAiTranslation(t);
        return;
    }
    finishAiJob(t, next, job->plan.translate && job->plan.combined);
}

void TranscriptionService::runAiTranslation(const Ticket& t) {
    const AiJob* job = m_aiJobs.find(t.seq);
    if (!job) return;
    TextPolishService::RequestParams params = aiRequestParams();
    params.model = job->plan.translateModel;
    params.targetLang = job->target;
    params.translateOnly = true;
    if (m_glossaryProvider) params.glossary = m_glossaryProvider(job->target);
    m_textPolishService->polishText(job->current, params,
        [this, t](bool ok, const QString& translated, const QString& error) { onAiTranslateDone(t, ok, translated, error); });
}

void TranscriptionService::onAiTranslateDone(const Ticket& t, bool ok, const QString& translated, const QString& error) {
    const auto job = m_aiJobs.take(t.seq);
    if (!job) return;   // 同上：已被超时兜底结束
    // 翻译失败或返回空串时保留翻译前的文字，不能把一整句吞掉
    const auto outcome = decideTranslationOutcome(ok && !translated.trimmed().isEmpty(), translated, job->current);
    if (outcome.usedFallback) {
        LOG_WARN(QString("AI translation failed, fallback to untranslated text: %1").arg(error));
        emit translationFailed(error.isEmpty() ? QStringLiteral("翻译结果为空") : explainOllamaError(error));
    } else {
        m_translationSources.insert(t.seq, job->current);
    }
    finish(t, true, outcome.text, {});
}

// 正常结束：取走状态，需要时记下翻译前原文（合并请求时译文就在 text 里，原文是识别原文），再交给 finish
void TranscriptionService::finishAiJob(const Ticket& t, const QString& text, bool translated) {
    const auto job = m_aiJobs.take(t.seq);
    if (!job) return;
    if (translated) m_translationSources.insert(t.seq, job->source);
    finish(t, true, text, {});
}

void TranscriptionService::transcribeGemini(const QByteArray &wavBytes, const Ticket& t) {
    TextPolishService::RequestParams p;
    p.apiKey = m_config.polish.apiKey;
    p.model = m_config.polish.model;
    p.customUrl = m_config.polish.apiUrl;
    p.targetLang = t.fileMode ? QStringLiteral("不翻译") : m_config.polish.cloudTargetLang();
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
        runAiStages(t, text);
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
    static const int kMaxAttempts = 60;
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
            runAiStages(t, text);
            return;
        }
        if (status == QStringLiteral("error")) {
            finish(t, false, {}, QStringLiteral("Gladia 转录失败"));
            return;
        }
        QTimer::singleShot(1000, this, [this, resultUrl, attempt, t]() {
            pollGladiaResult(resultUrl, attempt + 1, t);
        });
    });
}

void TranscriptionService::finish(const Ticket& t, bool success, const QString& rawText, const QString& errorMsg) {
    // 同一序号只处理一次；翻译中的超时交给 TranslationService 自己的超时
    if (!m_results.isPending(t.seq) || m_translating.contains(t.seq)) return;

    const QString cloudSource = m_translationSources.take(t.seq);
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
        completeResult(t, true, rawText, processed, {}, cloudSource);
        return;
    }

    showSourceFirst(t.seq, processed);
    m_translating.insert(t.seq);
    m_translationService->translate(processed, stage.from, stage.to,
        [this, t, rawText, processed](bool ok, const QString& translated, const QString& error) {
            m_translating.remove(t.seq);
            const auto outcome = decideTranslationOutcome(ok, translated, processed);
            if (outcome.usedFallback) {
                LOG_WARN(QString("Translation failed, fallback to original: %1").arg(error));
                emit translationFailed(error);
            }
            completeResult(t, true, rawText, outcome.text, {}, outcome.usedFallback ? QString() : processed);
        });
}

void TranscriptionService::showSourceFirst(quint64 seq, const QString& text) {
    m_pendingShown.insert(seq);
    emit transcriptionPending(seq, text);
}

void TranscriptionService::completeResult(const Ticket& t, bool success, const QString& rawText,
                                           const QString& finalText, const QString& errorMsg,
                                           const QString& sourceText) {
    const Result r{ success, rawText, finalText, errorMsg, t.fileMode, sourceText, t.seq };
    if (!m_results.complete(t.seq, r)) return;

    for (const Result& ready : m_results.takeReady()) {
        if (ready.fileMode)
            emit fileSegmentFinished(ready.success, ready.rawText, ready.finalText, ready.errorMsg);
        else {
            // 先于结果发出：之前已先显示过原文的句子，这里补上译文（或回退 / 失败的结局）；
            // 没先显示过但翻译成功的，也带着原文和译文一起给界面
            const bool shown = m_pendingShown.remove(ready.seq);
            const bool translated = ready.success && !ready.sourceText.isEmpty() && ready.sourceText != ready.finalText;
            if (shown || translated) {
                emit transcriptionTranslated(ready.seq, ready.sourceText.isEmpty() ? ready.finalText : ready.sourceText,
                                             ready.success ? ready.finalText : QString());
            }
            emit transcriptionFinished(ready.success, ready.rawText, ready.finalText, ready.errorMsg);
        }
    }
}

void TranscriptionService::armTimeout(const Ticket& t) {
    QTimer::singleShot(kNetworkTimeoutMs, this, [this, t]() {
        // 翻译阶段由 TranslationService 的独立超时兜底，别被这个更长的识别超时抢先判失败
        if (m_translating.contains(t.seq)) return;

        // 润色 / 翻译阶段超时：识别原文已经拿到，回退当前阶段的输入继续，不能当成识别失败
        if (const auto job = m_aiJobs.take(t.seq)) {
            const QString cause = QStringLiteral("请求超时，无响应");
            LOG_WARN(QString("Text polish timeout, fallback to original: %1").arg(cause));
            const auto outcome = decideEnhancementOutcome(false, job->current, {});
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

bool TranscriptionService::fileTranslationActive() const {
    const PolishConfig& p = m_config.polish;
    if (!TextPolishPrompts::hasTranslationTarget(p.activeTargetLang())) return false;
    if (p.usesLocalTranslation()) return m_translationService && m_translationService->hasBackend();
    return p.cloudEngineConfigured() && m_textPolishService->capabilities(p.aiEngineIndex).canTranslate;
}

void TranscriptionService::translateForFile(const QString& text, TranslateCallback done) {
    m_fileTranslateQueue.enqueue({text, std::move(done)});
    pumpFileTranslations();
}

void TranscriptionService::pumpFileTranslations() {
    // 本地翻译后端是单进程串行；云端并发 3 路，既不拖慢也不触发限流
    const int limit = m_config.polish.usesLocalTranslation() ? 1 : 3;
    while (m_fileTranslateInFlight < limit && !m_fileTranslateQueue.isEmpty()) {
        FileTranslateJob job = m_fileTranslateQueue.dequeue();
        ++m_fileTranslateInFlight;
        // 回调可能同步触发（源语言与目标语言相同），所以先计数再发；结束时补位继续发下一条
        dispatchFileTranslation(job.text, [this, done = std::move(job.done)](bool ok, const QString& result,
                                                                          const QString& error) {
            --m_fileTranslateInFlight;
            done(ok, result, error);
            pumpFileTranslations();
        });
    }
}

void TranscriptionService::dispatchFileTranslation(const QString& text, TranslateCallback done) {
    const PolishConfig& p = m_config.polish;
    if (p.usesLocalTranslation()) {
        // 复用听写的语言判定；fileMode 传 false，否则判定会直接跳过翻译
        const auto stage = decideTranslationStage(false, true, p.translateSourceLang, p.activeTargetLang(),
                                                  LanguageCatalog::codeOf(m_config.sherpa.languageModel));
        if (!stage.shouldTranslate) {
            done(true, text, {});
            return;
        }
        if (!m_translationService) {
            done(false, {}, QStringLiteral("翻译后端不可用"));
            return;
        }
        m_translationService->translate(text, stage.from, stage.to, std::move(done));
        return;
    }

    TextPolishService::RequestParams params;
    params.aiEngine = p.aiEngineIndex;
    params.apiKey = p.apiKey;
    params.customUrl = p.apiUrl;
    params.model = p.effectiveTranslateModel();
    params.targetLang = p.activeTargetLang();
    params.translateOnly = true;
    if (m_glossaryProvider) params.glossary = m_glossaryProvider(params.targetLang);

    // 云端请求没有自带超时；结果和超时谁先到用谁，回调只走一次
    auto pending = std::make_shared<TranslateCallback>(std::move(done));
    auto finishOnce = [pending](bool ok, const QString& result, const QString& error) {
        if (!*pending) return;
        TranslateCallback cb = std::move(*pending);
        *pending = nullptr;
        cb(ok, result, error);
    };
    QTimer::singleShot(kNetworkTimeoutMs, this, [finishOnce]() {
        finishOnce(false, {}, QStringLiteral("请求超时，无响应"));
    });
    m_textPolishService->polishText(text, params, finishOnce);
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

