#pragma once
#include <QObject>
#include <QByteArray>
#include <QVector>
#include <QString>
#include "config/AppConfig.h"

// 录音层依赖接口：WorkflowManager 只通过此接口驱动录音、接收上行信号。
class IRecorder : public QObject {
    Q_OBJECT
public:
    explicit IRecorder(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IRecorder() = default;

    virtual bool startListening() = 0;
    virtual void stopListening() = 0;
    virtual bool isListening() const = 0;
    virtual bool isPaused() const = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void updateConfig() = 0;

signals:
    void utteranceReady(const QByteArray &pcmData, int sampleRate);
    void levelUpdated(float rmsNormalized);
    void spectrumUpdated(const QVector<float> &bands);
    void voiceStarted();
    void voiceStopped();
    void errorOccurred(const QString& title, const QString& cause = {});
};

// 转写层依赖接口：WorkflowManager 只通过此接口发起转写、接收结果上行。
class ITranscription : public QObject {
    Q_OBJECT
public:
    explicit ITranscription(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ITranscription() = default;

    virtual void transcribe(const QByteArray &pcmData, int sampleRate,
                            int channels, int bitsPerSample) = 0;

signals:
    void transcriptionFinished(bool success, const QString &rawText,
                               const QString &finalText, const QString &errorMsg);
    // 要翻译的句子识别出来后立刻发出：界面先显示原文，不用等翻译（慢的翻译也不会让原文迟迟不出现）。
    // seq 是这一句在识别服务里的序号，后面的 transcriptionTranslated 用它对回同一句
    void transcriptionPending(quint64 seq, const QString &sourceText);
    // 这一句的翻译阶段结束（成功、回退原文或失败都会发），在 transcriptionFinished 之前发出（同一顺序）。
    // finalText 为空表示这一句识别失败
    void transcriptionTranslated(quint64 seq, const QString &sourceText, const QString &finalText);
};

// 模型层依赖接口：WorkflowManager 只通过此接口加载/查询模型、接收加载完成上行。
class ISherpaModel : public QObject {
    Q_OBJECT
public:
    explicit ISherpaModel(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ISherpaModel() = default;

    virtual bool reloadModel(const AppConfig &config) = 0;
    virtual void loadModelAsync(const AppConfig &config, bool isReload) = 0;
    virtual bool isModelLoaded() const = 0;
    virtual void pauseIdleTimer() = 0;
    virtual void resumeIdleTimer() = 0;

signals:
    void modelLoadFinished(bool success);
};
