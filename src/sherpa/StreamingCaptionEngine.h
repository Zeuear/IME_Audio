#pragma once
#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>

#include "SherpaConfig.h"

class QThread;

// @brief 实时字幕用的流式识别引擎
class StreamingCaptionEngine : public QObject
{
    Q_OBJECT
public:
    StreamingCaptionEngine();
    ~StreamingCaptionEngine() override;

    bool isReady() const { return m_ready.load(); }
    QString loadedRepoId() const;

public slots:
    void loadModel(const QString &repoId, int numThreads);
    void unloadModel();
    // 16 kHz / mono / 16-bit PCM
    void acceptAudio(const QByteArray &pcm);
    void resetStream(quint64 epoch);

signals:
    void partialText(quint64 epoch, const QString &text);
    // 运行中解码出错也发 ok=false，使用方据此关掉音频抽头
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
