#pragma once
#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>

#include "SherpaConfig.h"

class QThread;

// @brief 实时字幕用的流式识别引擎：独立的 OnlineRecognizer 加一个常驻 OnlineStream，只产出中间结果，
//        正式文本仍由 SherpaManager / 云端后端给出。
//
// 不放进 SherpaManager 的串行队列：那里一次转录可能占数秒，字幕解码会被挤到句末才出。
// 槽都在引擎自己的线程里执行，其它线程请走队列连接。
class StreamingCaptionEngine : public QObject
{
    Q_OBJECT
public:
    // 不接受 parent：构造时就要 moveToThread，而有父对象的 QObject 不能换线程，由使用方显式 delete
    StreamingCaptionEngine();
    ~StreamingCaptionEngine() override;

    bool isReady() const { return m_ready.load(); }
    QString loadedRepoId() const;

public slots:
    void loadModel(const QString &repoId, int numThreads);
    void unloadModel();
    // 16 kHz / mono / 16-bit PCM
    void acceptAudio(const QByteArray &pcm);
    // 一句话结束：换一个新流，此后的中间结果带新的 epoch
    void resetStream(quint64 epoch);

signals:
    void partialText(quint64 epoch, const QString &text);
    void modelReady(const QString &repoId, bool ok);

private:
    QThread *m_thread = nullptr;
    std::unique_ptr<sherpa_onnx::cxx::OnlineRecognizer> m_recognizer;
    std::unique_ptr<sherpa_onnx::cxx::OnlineStream> m_stream;
    std::atomic<bool> m_ready{false};
    QString m_repoId;
    mutable QMutex m_repoMutex;
    quint64 m_epoch = 0;
    QString m_lastText;
};
