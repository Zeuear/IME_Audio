#include "TranscriptionService.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>
#include <QMessageBox>
#include "TermsLibraryManager.h"
#include "utils/Logger.h"

TranscriptionService::TranscriptionService(
    QNetworkAccessManager* networkManager, 
    SherpaManager* sherpaManager,
    GeminiClient* geminiClient,
    const AppConfig& config, QObject *parent)
    : QObject(parent),
      m_config(config),
      m_manager(networkManager),
      m_sherpaManager(sherpaManager),
      m_geminiClient(geminiClient) {

    connect(m_geminiClient, &GeminiClient::polishFinished, this, [&](bool success, const QString& text, const QString& error) {
        emit transcriptionFinished(success, text, success ? postProcess(text) : QString(), error);
    });

    connect(m_geminiClient, &GeminiClient::transcribeFinished, this,
            [this](bool ok, const QString &text, const QString &err) {
                emit transcriptionFinished(ok, text, ok ? postProcess(text) : QString(), err);
            });

    connect(m_sherpaManager, &SherpaManager::utteranceTranscribed, this,
            [this](bool ok, const QString &text, const QString &err) {
                if (m_config.gemini.enableGemini) {
                    GeminiClient::RequestParams params;
                    params.aiEngine = m_config.gemini.aiEngineIndex;
                    params.apiKey = m_config.gemini.apiKey;
                    params.customUrl = m_config.gemini.apiUrl;
                    params.model = m_config.gemini.model;
                    params.style = m_config.gemini.geminiStyle;
                    params.customPrompt = m_config.gemini.prompt;
                    params.aiVocab = m_config.gemini.vocab;
                    m_geminiClient->polishText(text, params);
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
    GeminiClient::RequestParams p;
    p.apiKey = m_config.gemini.apiKey;
    p.model = m_config.gemini.model;
    p.customUrl = m_config.gemini.apiUrl;
    p.targetLang = m_config.gemini.targetLang;
    p.style = m_config.gemini.geminiStyle;
    p.customPrompt = m_config.gemini.prompt;
    p.replaceRules = m_config.replaceRules;
    p.aiVocab = m_config.gemini.vocab;
    p.aiEngine = m_config.gemini.aiEngineIndex;
    m_geminiClient->transcribeAudio(wavBytes, p);
}

void TranscriptionService::transcribeGroq(const QByteArray &wavBytes) {
    // Groq Whisper API：multipart/form-data 上传音频文件
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
    req.setRawHeader("Authorization", ("Bearer " + m_config.gemini.apiKey).toUtf8()); // TODO: 使用独立 groqKey 字段

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
    // Gladia API：预签名上传 + 转录请求，流程与 Groq 类似，此处按需扩展
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    auto* buffer = new QBuffer(multiPart);
    buffer->setData(wavBytes);
    buffer->open(QIODevice::ReadOnly);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant(QStringLiteral("form-data; name=\"audio\"; filename=\"audio.wav\"")));
    filePart.setBodyDevice(buffer);
    multiPart->append(filePart);

    QNetworkRequest req(QUrl("https://api.gladia.io/v2/transcription"));
    req.setRawHeader("x-gladia-key", m_config.gladiaKey.toUtf8());

    QNetworkReply *reply = m_manager->post(req, multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit transcriptionFinished(false, {}, {}, reply->errorString());
            return;
        }
        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        QString text = obj.value("prediction").toString();
        emit transcriptionFinished(true, text, postProcess(text), {});
    });
}

QString TranscriptionService::postProcess(const QString &rawText) {
    QString text = rawText;
    text = TermsLibraryManager::applyReplaceRules(text, m_config.replaceRules);
    text = applySimpleSherpaPunctuation(text);
    return text.trimmed();
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


QString TranscriptionService::applySimpleSherpaPunctuation(const QString& text) {
    if (text.isEmpty()) {
        return QString();
    }
    if (m_config.backend != AsrBackendKind::Sherpa) {
        return text;
    }

    QString finalText = text.trimmed();
    if (finalText.isEmpty() || textEndsWithSentencePunctuation(text)) {
        return text;
    }

    QString suffix = pickAutoPunctuationSuffix(text);
    finalText.append(suffix);
    LOG_DEBUG(QString("Auto punctuation appended for sherpa text suffix =").arg(suffix));
    return finalText;
}

bool TranscriptionService::textEndsWithSentencePunctuation(const QString& text) {
    QString cleanText = text.trimmed();
    if (cleanText.isEmpty()) {
        return false;
    }

    QChar lastChar = cleanText.at(cleanText.length() - 1);
    if (lastChar == '.' || lastChar == '!' || lastChar == '?' ||
        lastChar == ',' || lastChar == ';' || lastChar == ':') {
        return true;
    }

    if (cleanText.endsWith("。") ||
        cleanText.endsWith("！") ||
        cleanText.endsWith("？") ||
        cleanText.endsWith("，") ||
        cleanText.endsWith("；") ||
        cleanText.endsWith("：")) {
        return true;
    }
    return false;
}

QString TranscriptionService::pickAutoPunctuationSuffix(const QString& text) {
    static const QStringList questionSuffixes = { "吗", "么", "嘛", "呢" };
    static const QStringList questionKeywords = {
        "什么", "怎么", "为什么", "为啥", "是否", "是不是", "能不能", "可不可以",
        "要不要", "有没有", "行不行", "哪儿", "哪里", "谁", "多久", "多少", "几点", "几号"
    };

    static const QStringList exclaimSuffixes = { "啊", "呀", "哇", "啦" };
    static const QStringList exclaimKeywords = { "太好了", "太棒了", "真棒", "好厉害", "牛啊" };
    if (text.isEmpty()) {
        return ".";
    }

    QString cleanText = text.trimmed();
    if (cleanText.isEmpty()) {
        return ".";
    }

    bool hasNonAscii = false;
    for (const QChar& ch : cleanText) {
        if (ch.unicode() > 127) {
            hasNonAscii = true;
            break;
        }
    }
    for (const QString& suffix : questionSuffixes) {
        if (cleanText.endsWith(suffix)) {
            return hasNonAscii ? "？" : "?";
        }
    }
    for (const QString& keyword : questionKeywords) {
        if (cleanText.contains(keyword)) {
            return hasNonAscii ? "？" : "?";
        }
    }
    for (const QString& suffix : exclaimSuffixes) {
        if (cleanText.endsWith(suffix)) {
            return hasNonAscii ? "！" : "!";
        }
    }
    for (const QString& keyword : exclaimKeywords) {
        if (cleanText.contains(keyword)) {
            return hasNonAscii ? "！" : "!";
        }
    }
    return hasNonAscii ? "。" : ".";
}
