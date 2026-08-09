#pragma once
#include <QObject>
#include <QByteArray>
#include <QVector>
#include <QString>
#include "AppConfig.h"

// 录音层依赖接口：WorkflowManager 只通过此接口驱动录音、接收上行信号。
// 具体实现 = AudioRecorderService；测试用 FakeRecorder 注入，完全脱离 WASAPI/COM。
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
    virtual void playTestTone() = 0;

signals:
    void utteranceReady(const QByteArray &pcmData, int sampleRate);
    void levelUpdated(float rmsNormalized);
    void spectrumUpdated(const QVector<float> &bands);
    void voiceStarted();
    void voiceStopped();
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
