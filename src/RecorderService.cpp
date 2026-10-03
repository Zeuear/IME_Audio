#include "RecorderService.h"
#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioSink>
#include <QAudioSource>
#include <QBuffer>
#include <QCoreApplication>
#include <QPointer>
#include <QThread>
#include <QtMath>
#include <cmath>

#include "audio/AudioFormatConverter.h"
#include "audio/SpectrumWorker.h"
#include "audio/SpeechSegmenter.h"
#include "utils/Logger.h"
#include "utils/PlatformPermissions.h"

namespace {

bool isCustomDevice(const QString& name)
{
    return !name.isEmpty() && name != AudioConfig::kDefaultDeviceName;
}

// 按设置里的设备名称查找；选的是「默认」或设备已拔掉时退回系统默认设备
QAudioDevice findDevice(const QList<QAudioDevice>& devices, const QAudioDevice& fallback, const QString& name)
{
    if (!isCustomDevice(name)) return fallback;
    for (const auto& d : devices) {
        if (d.description() == name) return d;
    }
    return fallback;
}

}


AudioRecorderService::AudioRecorderService(const AppConfig& config, QObject* parent)
    : IRecorder(parent), m_config(config)
{
    m_spectrumThread = new QThread(this);
    m_spectrumWorker = new SpectrumWorker(m_config.audio.sampleRate);
    m_spectrumWorker->moveToThread(m_spectrumThread);
    connect(m_spectrumThread, &QThread::finished, m_spectrumWorker, &QObject::deleteLater);
    connect(m_spectrumWorker, &SpectrumWorker::spectrumReady, this, &AudioRecorderService::spectrumUpdated);
    connect(m_spectrumWorker, &SpectrumWorker::levelUpdated, this, &AudioRecorderService::levelUpdated);
    m_spectrumThread->start();

    m_segmenterThread = new QThread(this);
    m_segmenter = new SpeechSegmenter(config, m_config.audio.sampleRate);
    m_segmenter->moveToThread(m_segmenterThread);
    connect(m_segmenterThread, &QThread::finished, m_segmenter, &QObject::deleteLater);
    connect(m_segmenter, &SpeechSegmenter::speechStarted, this, &AudioRecorderService::onSpeechStarted);
    connect(m_segmenter, &SpeechSegmenter::speechEnded, this, &AudioRecorderService::onSpeechEnded);
    connect(m_segmenter, &SpeechSegmenter::sentenceReady, this, &AudioRecorderService::onSentenceReady);
    connect(m_segmenter, &SpeechSegmenter::errorOccurred, this, &AudioRecorderService::errorOccurred);
    m_segmenterThread->start();
}

AudioRecorderService::~AudioRecorderService()
{
    if (m_audioSink) { m_audioSink->stop(); delete m_audioSink; m_audioSink = nullptr; }
    m_spectrumThread->quit();
    m_spectrumThread->wait();
    m_segmenterThread->quit();
    m_segmenterThread->wait();
}

void AudioRecorderService::updateConfig()
{
    QMetaObject::invokeMethod(m_segmenter, "reloadDetector", Qt::QueuedConnection);
}

bool AudioRecorderService::startListening()
{
    if (m_audioSource) return true;
    if (!ensureMicrophonePermission()) return false;
    if (!ensureSegmenterReady()) return false;
    if (!openMicrophone()) return false;

    connect(m_audioDevice, &QIODevice::readyRead, this, &AudioRecorderService::onAudioDataReady);
    m_isPaused = false;
    m_singleUtterance.clear();
    m_voiceActive = false;

    emit voiceStarted();
    return true;
}

void AudioRecorderService::stopListening()
{
    if (!m_audioSource) return;
    m_voiceActive = false;

    readAndDispatch();

    // 恢复系统原来的默认播放/输入设备
    m_endpointController.restore();
    if (m_config.continuousMode) {
        emitUnfinishedSentences();
    } else {
        emitSingleUtterance();
    }

    m_audioSource->stop();
    m_audioSource->deleteLater();
    m_audioSource = nullptr;
    m_audioDevice = nullptr;
    m_singleUtterance.clear();
    m_isPaused = false;
    QMetaObject::invokeMethod(m_spectrumWorker, "resetLevel", Qt::QueuedConnection);
}

void AudioRecorderService::pause()
{
    if (!m_audioSource || m_isPaused) return;
    m_audioSource->suspend();
    m_isPaused = true;
    QMetaObject::invokeMethod(m_spectrumWorker, "resetLevel", Qt::QueuedConnection);
}

void AudioRecorderService::resume()
{
    if (!m_audioSource || !m_isPaused) return;
    m_audioSource->resume();
    m_isPaused = false;
}

bool AudioRecorderService::isListening() const { return m_audioSource != nullptr; }
bool AudioRecorderService::isPaused() const { return m_isPaused; }
bool AudioRecorderService::isVoiceActive() const { return m_voiceActive.load(); }

bool AudioRecorderService::ensureMicrophonePermission()
{
    using Status = PlatformPermissions::Status;

    const Status status = PlatformPermissions::microphoneStatus();
    LOG_DEBUG(QString("Recorder | microphone permission status=%1").arg(static_cast<int>(status)));
    if (status == Status::Granted || status == Status::NotRequired) {
        return true;
    }

    if (status == Status::NotDetermined) {
        // 系统授权框是异步的，本次启动只能先失败：用户点「允许」之后再触发一次热键即可。
        PlatformPermissions::requestMicrophoneAccess([](bool granted) {
            LOG_INFO(QString("Microphone permission request finished: %1")
                         .arg(granted ? "granted" : "denied"));
        });
        emit errorOccurred(tr("录音启动失败"),
                           tr("请在系统弹出的授权框中允许使用麦克风，然后重新开始录音"));
        return false;
    }

    // 已被拒绝：系统不会再弹框，唯一的出路是用户自己去隐私面板打开，直接带他过去。
    LOG_ERROR("Microphone permission denied");
    PlatformPermissions::openMicrophoneSettings();
    emit errorOccurred(tr("录音启动失败"),
                       tr("麦克风权限已被拒绝，请在「系统设置 → 隐私与安全性 → 麦克风」中允许本应用后重试"));
    return false;
}

bool AudioRecorderService::ensureSegmenterReady()
{
    if (!m_config.continuousMode.load() || m_segmenter->isReady()) return true;

    QMetaObject::invokeMethod(m_segmenter, "reloadDetector", Qt::BlockingQueuedConnection);
    if (m_segmenter->isReady()) return true;
    LOG_ERROR("Recorder | VAD not ready, continuous mode cannot start");
    return false;
}

bool AudioRecorderService::openMicrophone()
{
    QAudioFormat format;
    format.setSampleRate(m_config.audio.sampleRate);
    format.setChannelCount(m_config.audio.channels);
    format.setSampleFormat(m_config.audio.bitsPerSample == 8 ? QAudioFormat::UInt8 : QAudioFormat::Int16);

    const QAudioDevice device = findDevice(QMediaDevices::audioInputs(),
        QMediaDevices::defaultAudioInput(), m_config.audio.deviceName);

    if (isCustomDevice(m_config.audio.outputDeviceName)) {
        const QAudioDevice outputDevice = findDevice(QMediaDevices::audioOutputs(),
            QMediaDevices::defaultAudioOutput(), m_config.audio.outputDeviceName);
        m_endpointController.setDefaultOutput(outputDevice.id().toStdString());
    }
    if (isCustomDevice(m_config.audio.deviceName)) {
        m_endpointController.setDefaultInput(device.id().toStdString());
    }

    LOG_DEBUG(QString("Recorder | input device: %1 (id=%2, null=%3), configured name='%4'")
                  .arg(device.description(), QString::fromUtf8(device.id())).arg(device.isNull()).arg(m_config.audio.deviceName));
    if (device.isNull()) {
        LOG_ERROR("Recorder | 没有可用的音频输入设备");
    }

    if (!device.isFormatSupported(format)) {
        LOG_DEBUG(QString("Default format not supported (rate=%1 ch=%2 fmt=%3), trying to use the nearest.")
                      .arg(format.sampleRate()).arg(format.channelCount()).arg(static_cast<int>(format.sampleFormat())));
        format = device.preferredFormat();
    }

    m_audioSource = new QAudioSource(device, format, this);
    // 缓冲区是设备侧的，必须按设备实际格式算
    m_audioSource->setBufferSize(static_cast<qsizetype>(format.bytesForDuration(800 * 1000)));
    connect(m_audioSource, &QAudioSource::stateChanged, this, [this](QAudio::State state) {
        LOG_DEBUG(QString("Recorder | QAudioSource state=%1 error=%2")
                      .arg(static_cast<int>(state)).arg(m_audioSource ? static_cast<int>(m_audioSource->error()) : -1));
    });
    m_audioDevice = m_audioSource->start();
    if (!m_audioDevice) {
        LOG_ERROR(QString("Recorder | QAudioSource::start failed (state=%1, error=%2)，"
                          "若麦克风授权正常，请检查该设备是否被其它应用独占")
                      .arg(static_cast<int>(m_audioSource->state())).arg(static_cast<int>(m_audioSource->error())));
        m_audioSource->deleteLater();
        m_audioSource = nullptr;
        return false;
    }

    m_deviceFormat = m_audioSource->format();
    m_needsConversion = !AudioFormatConverter::isInternalFormat(m_deviceFormat, m_config.audio.sampleRate);
    if (m_needsConversion) {
        LOG_WARN(QString("Recorder | 设备实际格式 %1Hz/%2ch/fmt=%3 与识别期望 %4Hz/1ch/Int16 不同，已启用格式转换")
                     .arg(m_deviceFormat.sampleRate()).arg(m_deviceFormat.channelCount())
                     .arg(static_cast<int>(m_deviceFormat.sampleFormat())).arg(m_config.audio.sampleRate));
    }
    LOG_DEBUG(QString("Sample Rate: %1").arg(m_deviceFormat.sampleRate()));
    LOG_DEBUG(QString("Channels: %1").arg(m_deviceFormat.channelCount()));
    LOG_DEBUG(QString("Sample Format: %1").arg(static_cast<int>(m_deviceFormat.sampleFormat())));
    return true;
}

void AudioRecorderService::onAudioDataReady()
{
    readAndDispatch();
}

void AudioRecorderService::readAndDispatch()
{
    if (!m_audioDevice) return;
    QByteArray chunk = m_audioDevice->readAll();
    if (chunk.isEmpty()) return;
    if (m_needsConversion) {
        chunk = AudioFormatConverter::toInternalPcm(chunk, m_deviceFormat, m_config.audio.sampleRate);
        if (chunk.isEmpty()) return;
    }

    QMetaObject::invokeMethod(m_spectrumWorker, "processChunk", Qt::QueuedConnection,
        Q_ARG(QByteArray, chunk));

    if (m_config.continuousMode.load()) {
        QMetaObject::invokeMethod(m_segmenter, "processChunk", Qt::QueuedConnection,
            Q_ARG(QByteArray, chunk));
    } else {
        m_singleUtterance.append(chunk);
    }
}

void AudioRecorderService::onSpeechStarted()
{
    m_voiceActive = true;
    emit voiceStarted();
}

void AudioRecorderService::onSpeechEnded()
{
    m_voiceActive = false;
    emit voiceStopped();
}

// 连续模式
void AudioRecorderService::onSentenceReady(const QByteArray& pcmData, int sampleRate)
{
    if (m_config.continuousMode.load() && !pcmData.isEmpty()) {
        emit utteranceReady(pcmData, sampleRate);
    }
}

void AudioRecorderService::emitSingleUtterance()
{
    if (m_singleUtterance.isEmpty()) return;
    emit utteranceReady(m_singleUtterance, m_config.audio.sampleRate);
    emit voiceStopped();
    m_singleUtterance.clear();
}

void AudioRecorderService::emitUnfinishedSentences()
{
    // 阻塞调用，排在刚投递的 processChunk 之后执行，保证最后一块音频已经进了 VAD
    QList<QByteArray> unfinished;
    QMetaObject::invokeMethod(m_segmenter, [this, &unfinished]() { unfinished = m_segmenter->finishSession(); },
                              Qt::BlockingQueuedConnection);

    // 断句器之前已经发出、还排在主线程队列里的句子先送出，否则句子顺序会颠倒
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);

    for (const QByteArray& pcm : unfinished) {
        if (!pcm.isEmpty()) {
            emit utteranceReady(pcm, m_config.audio.sampleRate);
        }
    }
}

QStringList AudioRecorderService::availableMicrophones()
{
    QStringList names;
    names << AudioConfig::kDefaultDeviceName;
    for (const auto& dev : QMediaDevices::audioInputs())
        names << dev.description();
    return names;
}

QStringList AudioRecorderService::availableSpeakers()
{
    QStringList names;
    names << AudioConfig::kDefaultDeviceName;
    for (const auto& dev : QMediaDevices::audioOutputs())
        names << dev.description();
    return names;
}

void AudioRecorderService::playTestTone()
{
    const QAudioDevice outputDevice = findDevice(QMediaDevices::audioOutputs(),
        QMediaDevices::defaultAudioOutput(), m_config.audio.outputDeviceName);

    QAudioFormat fmt;
    fmt.setSampleRate(44100);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);

    // 清理上一次的 sink 与 buffer
    if (m_audioSink) { m_audioSink->stop(); delete m_audioSink; m_audioSink = nullptr; }
    if (m_audioBuffer) { delete m_audioBuffer; m_audioBuffer = nullptr; }

    m_audioSink = new QAudioSink(outputDevice, fmt, this);

    // 生成 0.3s 1kHz 正弦波
    const int sampleRate = fmt.sampleRate();
    const int n = sampleRate * 3 / 10;
    QByteArray data;
    data.resize(n * 2);
    int16_t* p = reinterpret_cast<int16_t*>(data.data());
    for (int i = 0; i < n; ++i) {
        double t = double(i) / sampleRate;
        p[i] = static_cast<int16_t>(0.3 * 32767 * std::sin(2.0 * M_PI * 1000.0 * t));
    }
    m_audioBuffer = new QBuffer(this);
    m_audioBuffer->setData(data);
    m_audioBuffer->open(QIODevice::ReadOnly);

    QPointer<QBuffer> bufPtr(m_audioBuffer);
    QObject::connect(m_audioSink, &QAudioSink::stateChanged, this,
    [this, bufPtr](QAudio::State s) mutable {
        if ((s == QAudio::IdleState || s == QAudio::StoppedState) && bufPtr) {
            bufPtr->deleteLater();
            m_audioBuffer = nullptr;
            if (m_audioSink) m_audioSink->deleteLater();
            m_audioSink = nullptr;
        }
    });
    m_audioSink->start(m_audioBuffer);
}
