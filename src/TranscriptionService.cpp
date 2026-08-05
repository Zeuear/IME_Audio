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
#include "utils/Logger.h"

TranscriptionService::TranscriptionService(
    QNetworkAccessManager* networkManager, 
    SherpaManager* sherpaManager,
    TextPolishService* textPolishService,
    const AppConfig& config, QObject *parent)
    : QObject(parent),
      m_config(config),
      m_manager(networkManager),
      m_sherpaManager(sherpaManager),
      m_textPolishService(textPolishService),
      m_textProcessor(new TextPostProcessor(this))
{

    connect(m_textPolishService, &TextPolishService::polishFinished, this, [&](bool success, const QString& text, const QString& error) {
        emit transcriptionFinished(success, text, success ? postProcess(text) : QString(), error);
    });

    connect(m_textPolishService, &TextPolishService::connectionTested, this,
        [this](bool success, const QString& message) {
            if (!success) {
                LOG_ERROR("测试连接失败!");
            }
            else {
                LOG_INFO("测试连接成功!");
            }
        });

    connect(m_sherpaManager, &SherpaManager::utteranceTranscribed, this,
            [this](bool ok, const QString &text, const QString &err) {
                if (m_config.polish.enableAiEnhancement) {
                    TextPolishService::RequestParams params;
                    params.aiEngine = m_config.polish.aiEngineIndex;
                    params.apiKey = m_config.polish.apiKey;
                    params.customUrl = m_config.polish.apiUrl;
                    params.model = m_config.polish.model;
                    params.style = m_config.polish.aiStyle;
                    params.customPrompt = m_config.polish.prompt;
                    params.aiVocab = m_config.polish.vocab;
                    params.targetLang = m_config.polish.targetLang;

                    m_textPolishService->polishText(text, params);
                }
                else {
                    emit transcriptionFinished(ok, text, ok ? postProcess(text) : QString(), err);
                }
            });
}

void TranscriptionService::transcribe(const QByteArray& pcmData, int sampleRate, int channels, int bitsPerSample){
    if (m_config.backend == AsrBackendKind::Sherpa) {
        transcribeSherpa(pcmData, sampleRate);
        return;
    }

    QByteArray wavBytes = buildWavBytes(pcmData, sampleRate, channels, bitsPerSample);
    switch (m_config.backend) {
    case AsrBackendKind::Groq:   transcribeGroq(wavBytes); break;
    case AsrBackendKind::Gladia: transcribeGladia(wavBytes); break;
    case AsrBackendKind::Gemini: transcribeGemini(wavBytes); break;
    default: break;
    }
}

void TranscriptionService::transcribeSherpa(const QByteArray& pcmData, int sampleRate) {
    QString text, error;
    m_sherpaManager->transcribeAsync(pcmData, sampleRate);
}

void TranscriptionService::transcribeGemini(const QByteArray &wavBytes) {
    TextPolishService::RequestParams p;
    p.apiKey = m_config.polish.apiKey;
    p.model = m_config.polish.model;
    p.customUrl = m_config.polish.apiUrl;
    p.targetLang = m_config.polish.targetLang;
    p.style = m_config.polish.aiStyle;
    p.customPrompt = m_config.polish.prompt;
    p.replaceRules = m_config.replaceRules;
    p.aiVocab = m_config.polish.vocab;
    p.aiEngine = m_config.polish.aiEngineIndex;

    GeminiProvider provider;
    provider.transcribe(wavBytes, p, m_manager, [this](bool ok, QString text, QString err) {
        emit transcriptionFinished(ok, text, ok ? postProcess(text) : QString(), err);
    });
}

void TranscriptionService::transcribeGroq(const QByteArray &wavBytes) {
    // Groq Whisper API：multipart/form-data 上传音频文件
    if (m_config.groqKey.isEmpty()) {
        emit transcriptionFinished(false, {}, {}, QStringLiteral("Groq API Key 未配置"));
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

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit transcriptionFinished(false, {}, {}, reply->errorString());
            return;
        }
        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QString text = obj.value("text").toString();
        emit transcriptionFinished(true, text, postProcess(text), {});
    });
}

void TranscriptionService::transcribeGladia(const QByteArray &wavBytes) {
    // Gladia v2 真实流程：上传拿 audio_url → 提交转录（异步）→ 轮询 result_url
    if (m_config.gladiaKey.isEmpty()) {
        emit transcriptionFinished(false, {}, {}, QStringLiteral("Gladia API Key 未配置"));
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

    connect(upReply, &QNetworkReply::finished, this, [this, upReply]() {
        upReply->deleteLater();
        if (upReply->error() != QNetworkReply::NoError) {
            emit transcriptionFinished(false, {}, {}, upReply->errorString());
            return;
        }
        QJsonObject upObj = QJsonDocument::fromJson(upReply->readAll()).object();
        QString audioUrl = upObj.value("audio_url").toString();
        if (audioUrl.isEmpty()) {
            emit transcriptionFinished(false, {}, {}, QStringLiteral("Gladia 上传失败：未返回 audio_url"));
            return;
        }
        submitGladiaTranscription(audioUrl);
    });
}

void TranscriptionService::submitGladiaTranscription(const QString &audioUrl) {
    QJsonObject body;
    body["audio_url"] = audioUrl;
    QJsonDocument doc(body);

    QNetworkRequest req(QUrl("https://api.gladia.io/v2/transcription"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-gladia-key", m_config.gladiaKey.toUtf8());

    QNetworkReply *reply = m_manager->post(req, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit transcriptionFinished(false, {}, {}, reply->errorString());
            return;
        }
        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QString resultUrl = obj.value("result_url").toString();
        if (resultUrl.isEmpty()) {
            emit transcriptionFinished(false, {}, {}, QStringLiteral("Gladia 转录提交失败：未返回 result_url"));
            return;
        }
        pollGladiaResult(resultUrl, 0);
    });
}

void TranscriptionService::pollGladiaResult(const QString &resultUrl, int attempt) {
    static const int kMaxAttempts = 60; // ~60s @ 1s
    if (attempt >= kMaxAttempts) {
        emit transcriptionFinished(false, {}, {}, QStringLiteral("Gladia 转录超时"));
        return;
    }

    QNetworkRequest req{ QUrl(resultUrl) };
    req.setRawHeader("x-gladia-key", m_config.gladiaKey.toUtf8());

    QNetworkReply *reply = m_manager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, resultUrl, attempt]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit transcriptionFinished(false, {}, {}, reply->errorString());
            return;
        }
        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QString status = obj.value("status").toString();
        if (status == QStringLiteral("done")) {
            QString text = obj.value("result").toObject()
                                .value("transcription").toObject()
                                .value("full_transcript").toString();
            emit transcriptionFinished(true, text, postProcess(text), {});
            return;
        }
        if (status == QStringLiteral("error")) {
            emit transcriptionFinished(false, {}, {}, QStringLiteral("Gladia 转录失败"));
            return;
        }
        // 仍处理中：1s 后重试
        QTimer::singleShot(1000, this, [this, resultUrl, attempt]() {
            pollGladiaResult(resultUrl, attempt + 1);
        });
    });
}

QString TranscriptionService::postProcess(const QString &rawText) {

    m_textProcessor->setReplaceRules(m_config.replaceRules);
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

