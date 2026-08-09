#pragma once
#include <QObject>
#include <QString>
#include "AppConfig.h"
#include "TextPolishService.h"
#include "sherpa/SherpaManager.h"
#include "TextPostProcessor.h"
#include "interfaces/workflow_interfaces.h"

// @brief  
// 统一转录入口：根据 backend 类型分发到 Groq / Sherpa / Gladia / Gemini
class TranscriptionService : public ITranscription {
    Q_OBJECT
public:
    explicit TranscriptionService(QNetworkAccessManager* networkManager, 
                                  SherpaManager* sherpaManager,
                                  TextPolishService* textPolishService,
                                  const AppConfig& config,
                                  QObject *parent = nullptr);

    void transcribe(const QByteArray& pcmData, int sampleRate, int channels, int bitsPerSample);

private:
    void transcribeGroq(const QByteArray &wavBytes);
    void transcribeGladia(const QByteArray &wavBytes);
    void submitGladiaTranscription(const QString &audioUrl);
    void pollGladiaResult(const QString &resultUrl, int attempt);
    void transcribeSherpa(const QByteArray& pcmData, int sampleRate);
    void transcribeGemini(const QByteArray &wavBytes);

    static QByteArray buildWavBytes(const QByteArray& pcmData, int sampleRate, int channels, int bitsPerSample);
    QString postProcess(const QString &rawText);

    TextPostProcessor* m_textProcessor;

    const AppConfig& m_config;
    QNetworkAccessManager* m_manager;
    TextPolishService* m_textPolishService;
    SherpaManager *m_sherpaManager;
};