#pragma once
#include <QObject>
#include <QString>
#include "config/AppConfig.h"
#include "TextPolishService.h"
#include "sherpa/SherpaManager.h"
#include "TextPostProcessor.h"
#include "interfaces/workflow_interfaces.h"
#include "textpolish/AiJobTracker.h"
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

    // 文件转录的译文：开关和目标语言沿用听写的翻译设置。
    // 本地离线翻译引擎走 TranslationService，云端引擎（需开启 AI 增强）发只翻译的请求。
    // 文件模式下转录结果本身保持原文，译文只经这里另外生成
    using TranslateCallback = std::function<void(bool ok, const QString& text, const QString& error)>;
    bool fileTranslationActive() const;
    QString fileTranslationTarget() const { return m_config.polish.activeTargetLang(); }
    // 请求排队发出（同时在途数有上限）：整部视频、多个文件的句子一次涌进来时，不排队会让本地后端
    // 后面的请求在队列里等满 30 秒超时，云端则撞上限流，这些句子都会没有译文
    void translateForFile(const QString& text, TranslateCallback done);

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
        QString sourceText;   // 翻译前的文字；没发生翻译时为空
        quint64 seq = 0;
    };
    // 网络请求无响应时的兜底，否则一句卡住会挡住后面所有结果；要长于上传加 Gladia 最多约 60 秒的轮询
    static constexpr int kNetworkTimeoutMs = 150 * 1000;

    void finish(const Ticket& t, bool success, const QString& rawText, const QString& errorMsg);
    void completeResult(const Ticket& t, bool success, const QString& rawText,
                        const QString& finalText, const QString& errorMsg, const QString& sourceText = {});
    void armTimeout(const Ticket& t);

    void transcribeGroq(const QByteArray &wavBytes, const Ticket& t);
    void transcribeGladia(const QByteArray &wavBytes, const Ticket& t);
    void submitGladiaTranscription(const QString &audioUrl, const Ticket& t);
    void pollGladiaResult(const QString &resultUrl, int attempt, const Ticket& t);
    void transcribeGemini(const QByteArray &wavBytes, const Ticket& t);
    void onSherpaTranscribed(bool ok, const QString& text, const QString& err);
    // 识别文字之后的 AI 阶段：润色、翻译各自按开关执行（见 AiStagePlan），本地离线翻译不在这里
    void runAiStages(const Ticket& t, const QString& text);
    void runAiPolish(const Ticket& t);
    void onAiPolishDone(const Ticket& t, bool ok, const QString& polished, const QString& error);
    void runAiTranslation(const Ticket& t);
    void onAiTranslateDone(const Ticket& t, bool ok, const QString& translated, const QString& error);
    void finishAiJob(const Ticket& t, const QString& text, bool translated);
    // 要翻译的句子先把原文交给界面，翻译结束时 completeResult 再补译文
    void showSourceFirst(quint64 seq, const QString& text);
    TextPolishService::RequestParams aiRequestParams() const;

    static QByteArray buildWavBytes(const QByteArray& pcmData, int sampleRate, int channels, int bitsPerSample);
    QString postProcess(const QString &rawText, bool fileMode);

    TextPostProcessor* m_textProcessor;
    bool m_fileMode = false;
    OrderedResults<Result> m_results;
    QQueue<Ticket> m_sherpaTickets;

    // 云端 AI 阶段进行中的句子：回调与超时兜底谁先 take 谁收尾，迟到的一方直接忽略
    AiJobTracker m_aiJobs;
    // 序号 → 云端翻译前的文字：finish 取走，随结果一起交给实时转录页的右栏
    QHash<quint64, QString> m_translationSources;
    struct FileTranslateJob {
        QString text;
        TranslateCallback done;
    };
    void pumpFileTranslations();
    void dispatchFileTranslation(const QString& text, TranslateCallback done);
    QQueue<FileTranslateJob> m_fileTranslateQueue;
    int m_fileTranslateInFlight = 0;
    QSet<quint64> m_pendingShown;     // 已经把原文先交给界面、还没发"翻译结束"的序号
    QSet<quint64> m_translating;      // 翻译中的序号；翻译服务有自己的超时，别被识别兜底抢先判失败

    const AppConfig& m_config;
    QNetworkAccessManager* m_manager;
    TextPolishService* m_textPolishService;
    TranslationService* m_translationService;
    SherpaManager *m_sherpaManager;
};