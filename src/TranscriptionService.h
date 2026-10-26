#pragma once
#include <QObject>
#include <QString>
#include "AppConfig.h"
#include "TextPolishService.h"
#include "sherpa/SherpaManager.h"
#include "TextPostProcessor.h"
#include "interfaces/workflow_interfaces.h"
#include "utils/OrderedResults.h"
#include <QQueue>
#include <QHash>
#include <QSet>
#include <functional>

class TranslationService;

// @brief  
// 统一转录入口：根据 backend 类型分发到 Groq / Sherpa / Gladia / Gemini
class TranscriptionService : public ITranscription {
    Q_OBJECT
public:
    explicit TranscriptionService(QNetworkAccessManager* networkManager, 
                                  SherpaManager* sherpaManager,
                                  TextPolishService* textPolishService,
                                  TranslationService* translationService,
                                  const AppConfig& config,
                                  QObject *parent = nullptr);

    void transcribe(const QByteArray& pcmData, int sampleRate, int channels, int bitsPerSample);

    // 用一个只读请求验证云端识别的 Key，不上传音频；cause 为中文失败原因
    using KeyTestCallback = std::function<void(bool ok, const QString& cause)>;
    void testKey(AsrBackendKind backend, const QString& key, KeyTestCallback done);

    // 文件转录模式
    void setFileMode(bool on) { m_fileMode = on; }
    bool fileMode() const { return m_fileMode; }

signals:
    void fileSegmentFinished(bool success, const QString &rawText,
                             const QString &finalText, const QString &errorMsg);
    // 润色失败已回退原文
    void enhancementFailed(const QString& cause, bool fileMode);
    // 本地翻译失败已回退翻译前文字（听写才会进入翻译）
    void translationFailed(const QString& cause);

private:
    // 一次转录请求。云端请求是并发的，结果可能乱序回来，按 seq 排好再发出；
    // fileMode 在提交时定下，避免结果到达时模式已切换、文件转录结果被注入前台窗口
    struct Ticket {
        quint64 seq = 0;
        bool fileMode = false;
    };
    struct Result {
        bool success = false;
        QString rawText, finalText, errorMsg;
        bool fileMode = false;
    };
    // 网络请求无响应时的兜底，否则一句卡住会挡住后面所有结果；要长于上传加 Gladia 最多约 60 秒的轮询
    static constexpr int kNetworkTimeoutMs = 150 * 1000;

    void finish(const Ticket& t, bool success, const QString& rawText, const QString& errorMsg);
    void completeResult(const Ticket& t, bool success, const QString& rawText,
                        const QString& finalText, const QString& errorMsg);
    void armTimeout(const Ticket& t);

    void transcribeGroq(const QByteArray &wavBytes, const Ticket& t);
    void transcribeGladia(const QByteArray &wavBytes, const Ticket& t);
    void submitGladiaTranscription(const QString &audioUrl, const Ticket& t);
    void pollGladiaResult(const QString &resultUrl, int attempt, const Ticket& t);
    void transcribeGemini(const QByteArray &wavBytes, const Ticket& t);
    void onSherpaTranscribed(bool ok, const QString& text, const QString& err);

    static QByteArray buildWavBytes(const QByteArray& pcmData, int sampleRate, int channels, int bitsPerSample);
    QString postProcess(const QString &rawText, bool fileMode);

    TextPostProcessor* m_textProcessor;
    bool m_fileMode = false;
    OrderedResults<Result> m_results;
    QQueue<Ticket> m_sherpaTickets;   // Sherpa 串行转录，结果按提交顺序回来
    // 序号 → 润色前文字：润色超时时用它回退；回调返回后即删除，迟到的结果不再处理
    QHash<quint64, QString> m_polishOriginals;
    QSet<quint64> m_translating;      // 翻译中的序号；翻译服务有自己的超时，别被识别兜底抢先判失败

    const AppConfig& m_config;
    QNetworkAccessManager* m_manager;
    TextPolishService* m_textPolishService;
    TranslationService* m_translationService;
    SherpaManager *m_sherpaManager;
};