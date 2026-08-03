#pragma once
#include <QObject>
#include <QString>
#include "AppConfig.h"
#include "GeminiClient.h"
#include "sherpa/SherpaManager.h"
#include "TextPostProcessor.h"

// @brief  
// 统一转录入口：根据 backend 类型分发到 Groq / Sherpa / Gladia / Gemini
class TranscriptionService : public QObject {
    Q_OBJECT
public:
    explicit TranscriptionService(QNetworkAccessManager* networkManager, 
                                  SherpaManager* sherpaManager,
                                  GeminiClient* geminiManager, 
                                  const AppConfig& config,
                                  QObject *parent = nullptr);

    void transcribe(const QByteArray& pcmData, int sampleRate, int channels, int bitsPerSample);

signals:
    void transcriptionFinished(bool success, const QString &rawText,
                                const QString &finalText, const QString &error);

private:
    void transcribeGroq(const QByteArray &wavBytes);
    void transcribeGladia(const QByteArray &wavBytes);
    void transcribeSherpa(const QByteArray& pcmData, int sampleRate);
    void transcribeGemini(const QByteArray &wavBytes);

    static QByteArray buildWavBytes(const QByteArray& pcmData, int sampleRate, int channels, int bitsPerSample);
    QString postProcess(const QString &rawText);

    TextPostProcessor* m_textProcessor;

    const AppConfig& m_config;
    QNetworkAccessManager* m_manager;
    GeminiClient *m_geminiClient;
    SherpaManager *m_sherpaManager;
};