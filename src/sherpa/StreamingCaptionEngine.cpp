#include "StreamingCaptionEngine.h"
#include <QMetaObject>
#include <QThread>
#include <vector>
#include "../utils/Logger.h"

StreamingCaptionEngine::StreamingCaptionEngine()
{
    m_thread = new QThread();
    m_thread->setObjectName(QStringLiteral("StreamingCaptionEngine"));
    moveToThread(m_thread);
    m_thread->start();
}

StreamingCaptionEngine::~StreamingCaptionEngine()
{
    // 识别器必须在自己的线程里释放
    if (m_thread->isRunning()) {
        QMetaObject::invokeMethod(this, &StreamingCaptionEngine::unloadModel, Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
    }
    delete m_thread;
}

QString StreamingCaptionEngine::loadedRepoId() const
{
    QMutexLocker lock(&m_repoMutex);
    return m_repoId;
}

void StreamingCaptionEngine::loadModel(const QString &repoId, int numThreads)
{
    if (m_ready.load() && loadedRepoId() == repoId) return;
    unloadModel();

    // 只用 CPU，GPU 留给正式转录
    auto result = ModelRegistry::GetConfig(repoId, numThreads, /*useGpu=*/false, /*allowHotwords=*/false);
    auto *online = std::get_if<std::unique_ptr<sherpa_onnx::cxx::OnlineRecognizer>>(&result.recognizer);
    if (!result.isLoaded || !online || !*online) {
        LOG_WARN(QString("Live caption model failed to load: %1").arg(repoId));
        emit modelReady(repoId, false);
        return;
    }

    try {
        m_recognizer = std::move(*online);
        m_stream = std::make_unique<sherpa_onnx::cxx::OnlineStream>(m_recognizer->CreateStream());
    } catch (const std::exception &e) {
        LOG_WARN(QString("Live caption stream creation failed: %1").arg(QString::fromLocal8Bit(e.what())));
        m_stream.reset();
        m_recognizer.reset();
        emit modelReady(repoId, false);
        return;
    }

    {
        QMutexLocker lock(&m_repoMutex);
        m_repoId = repoId;
    }
    m_lastText.clear();
    m_ready = true;
    LOG_INFO(QString("Live caption model ready: %1").arg(repoId));
    emit modelReady(repoId, true);
}

void StreamingCaptionEngine::unloadModel()
{
    const bool wasReady = m_ready.exchange(false);
    m_stream.reset();
    m_recognizer.reset();
    m_lastText.clear();
    {
        QMutexLocker lock(&m_repoMutex);
        m_repoId.clear();
    }
    if (wasReady) LOG_DEBUG("Live caption model unloaded");
}

void StreamingCaptionEngine::acceptAudio(const QByteArray &pcm)
{
    if (!m_ready.load() || !m_recognizer || !m_stream || pcm.isEmpty()) return;

    const auto *raw = reinterpret_cast<const int16_t *>(pcm.constData());
    const int count = static_cast<int>(pcm.size() / 2);
    std::vector<float> samples(count);
    for (int i = 0; i < count; ++i) samples[i] = raw[i] / 32768.0f;

    try {
        m_stream->AcceptWaveform(16000, samples.data(), count);
        while (m_recognizer->IsReady(m_stream.get())) {
            m_recognizer->Decode(m_stream.get());
        }
        const QString text = QString::fromStdString(m_recognizer->GetResult(m_stream.get()).text);
        if (text != m_lastText) {
            m_lastText = text;
            emit partialText(m_epoch, text);
        }
    } catch (const std::exception &e) {
        // 字幕只是辅助，出错就停用自己，不影响正式转录
        LOG_WARN(QString("Live caption decode failed, disabled: %1").arg(QString::fromLocal8Bit(e.what())));
        m_ready = false;
    }
}

void StreamingCaptionEngine::resetStream(quint64 epoch)
{
    m_epoch = epoch;
    m_lastText.clear();
    if (!m_ready.load() || !m_recognizer) return;
    try {
        // 换新流而不是 Reset()，不带上一句的解码状态
        m_stream = std::make_unique<sherpa_onnx::cxx::OnlineStream>(m_recognizer->CreateStream());
    } catch (const std::exception &e) {
        LOG_WARN(QString("Live caption reset failed, disabled: %1").arg(QString::fromLocal8Bit(e.what())));
        m_ready = false;
    }
}
