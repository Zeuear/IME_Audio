#include "RecorderService.h"
#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioSink>
#include <QAudioFormat>
#include <QBuffer>
#include <QPointer>
#include <QFile>
#include <QDateTime>
#include <QDataStream>
#include <QApplication>
#include <QDir>
#include <QThread>
#include <cstring>
#include "utils/Logger.h"
#include "utils/PlatformPermissions.h"
#include "utils/Diagnostics.h"
#include "utils/AppPaths.h"


SpectrumWorker::SpectrumWorker(int sampleRate):QObject(nullptr), m_sampleRate(sampleRate){
    m_kissFftCfg = kiss_fftr_alloc(kFftSize, 0, nullptr, nullptr);

    computeBandLayout();

    const float frameRateHz = static_cast<float>(m_sampleRate) / static_cast<float>(kFftSize);
    m_controller = std::make_unique<AdaptiveSpectrumController>(kBandCount, frameRateHz, m_bandCenterHz);
}
        
SpectrumWorker::~SpectrumWorker() {
    if (m_kissFftCfg) {
        kiss_fftr_free(static_cast<kiss_fftr_cfg>(m_kissFftCfg));
    }
};

void SpectrumWorker::computeBandLayout()
{
    const double binHz = static_cast<double>(m_sampleRate) / kFftSize;
    const int totalBins = kFftSize / 2 + 1;

    int minBin = qMax(1, static_cast<int>(kVoiceMinHz / binHz));
    int maxBin = qMin(totalBins - 1, static_cast<int>(kVoiceMaxHz / binHz));
    if (maxBin <= minBin) maxBin = minBin + 1;

    m_bandRanges.resize(kBandCount);
    m_bandCenterHz.resize(kBandCount);

    double logMin = std::log10(static_cast<double>(minBin));
    double logMax = std::log10(static_cast<double>(maxBin));

    for (int b = 0; b < kBandCount; ++b) {
        double lowLog = logMin + (logMax - logMin) * (static_cast<double>(b) / kBandCount);
        double highLog = logMin + (logMax - logMin) * (static_cast<double>(b + 1) / kBandCount);

        int lowBin = static_cast<int>(std::pow(10.0, lowLog));
        int highBin = static_cast<int>(std::pow(10.0, highLog));
        highBin = qMax(highBin, lowBin + 1);
        highBin = qMin(highBin, maxBin);
        lowBin = qMin(lowBin, highBin - 1);

        m_bandRanges[b] = { lowBin, highBin };

        double centerBin = (lowBin + highBin) / 2.0;
        m_bandCenterHz[b] = static_cast<float>(centerBin * binHz);
    }
}

void SpectrumWorker::processChunk(const QByteArray chunk)
{
    updateVadState(chunk);

    const int16_t* samples = reinterpret_cast<const int16_t*>(chunk.constData());
    const int count = chunk.size() / 2;

    for (int i = 0; i < count; ++i) {
        m_fftInputBuffer.push_back(samples[i] / 32768.0f);
    }

    if (static_cast<int>(m_fftInputBuffer.size()) < kFftSize) {
        return;
    }

    std::vector<float> window(m_fftInputBuffer.end() - kFftSize, m_fftInputBuffer.end());
    m_fftInputBuffer.clear();

    for (int i = 0; i < kFftSize; ++i) {
        float w = 0.5f - 0.5f * std::cos(2.0f * M_PI * i / (kFftSize - 1));
        window[i] *= w;
    }

    std::vector<kiss_fft_cpx> fftOut(kFftSize / 2 + 1);
    kiss_fftr(static_cast<kiss_fftr_cfg>(m_kissFftCfg), window.data(), fftOut.data());

    std::vector<float> magnitudes(kFftSize / 2 + 1);
    for (size_t i = 0; i < magnitudes.size(); ++i) {
        magnitudes[i] = std::sqrt(fftOut[i].r * fftOut[i].r + fftOut[i].i * fftOut[i].i);
    }

    // RMS（能量平均）
    std::vector<float> bandsDb(kBandCount);
    for (int b = 0; b < kBandCount; ++b) {
        const auto& range = m_bandRanges[b];
        double sumSq = 0.0;
        int binCount = 0;
        for (int i = range.lowBin; i < range.highBin && i < static_cast<int>(magnitudes.size()); ++i) {
            sumSq += static_cast<double>(magnitudes[i]) * magnitudes[i];
            ++binCount;
        }
        float rms = (binCount > 0) ? static_cast<float>(std::sqrt(sumSq / binCount)) : 0.0f;
        bandsDb[b] = 20.0f * std::log10(std::max(rms, 1e-6f));
    }

    // 分频段自适应量程 + 心理声学权重 + 全局响度增益
    const std::vector<float>& visualBands = m_controller->process(bandsDb, m_rmsLevel);

    float targetGate = m_vadVoiceActive ? 1.0f : 0.0f;
    QVector<float> qVec(visualBands.size());
    for (int i = 0; i < static_cast<int>(visualBands.size()); ++i) {
        qVec[i] = visualBands[i] * targetGate;
    }
    emit spectrumReady(qVec);
}

void SpectrumWorker::updateVadState(const QByteArray& chunk) {
    const int16_t* samples = reinterpret_cast<const int16_t*>(chunk.constData());
    const int count = chunk.size() / 2;
    if (count == 0) return;

    // 计算当前帧的瞬时 RMS
    double sumSquares = 0.0;
    for (int i = 0; i < count; ++i) {
        double v = static_cast<double>(samples[i]);
        sumSquares += v * v;
    }
    double instantRms = std::sqrt(sumSquares / count);

    // 转成 dBFS，映射到 [0,1]
    double instantDb = 20.0 * std::log10(std::max(instantRms, 1.0) / 32768.0);
    float instantNormalized = std::clamp(static_cast<float>((instantDb + 60.0) / 45.0), 0.0f, 1.0f);

    float dtMs = (static_cast<float>(count) / m_sampleRate) * 1000.0f;
    float targetMs = (instantNormalized > m_rmsLevel)
                    ? m_rmsEnvelopeParams.attackMs
                    : m_rmsEnvelopeParams.releaseMs;
    float alpha = 1.0f - std::exp(-dtMs / targetMs);

    m_rmsLevel += alpha * (instantNormalized - m_rmsLevel);
    if (m_vadVoiceActive) {
        float nextLevel = 0.50f * (m_rmsLevel / (m_rmsLevel + 0.50f));
        emit levelUpdated(nextLevel);
    }
    else {
        emit levelUpdated(0.0f);
    }
}

void SpectrumWorker::resetLevel() {
    m_rmsLevel = 0.0f;
    //m_vadVoiceActive = false;
    emit levelUpdated(m_rmsLevel);
}

void SpectrumWorker::onVadSpeechStarted()
{
    m_vadVoiceActive = true;
}

void SpectrumWorker::onVadSpeechEnded()
{
    m_vadVoiceActive = false;
}


VadWorker::VadWorker(const AppConfig& config, int sampleRate, QObject* parent)
    : QObject(parent), m_sampleRate(sampleRate), m_config(config)
{}


void VadWorker::rebuildDetector()
{
    // vadPath 是构造配置时解析的；VAD 模型可能是之后才下载好的，这里重新解析一次
    QString vadPath = m_config.sherpa.vadPath;
    if (!QFile::exists(vadPath)) {
        vadPath = AppPaths::vadModelFile();
    }
    if (!QFile::exists(vadPath)) {
        LOG_ERROR(QString("VAD model not found: %1").arg(vadPath));
        emit errorOccurred(tr("录音启动失败"), tr("VAD 模型缺失，正在自动下载，请稍候重试"));
        return;
    }
        
    sherpa_onnx::cxx::VadModelConfig vadConfig;
    vadConfig.silero_vad.model = vadPath.toStdString();
    vadConfig.silero_vad.threshold = static_cast<float>(m_config.audio.voiceThreshold) / 1000;
    vadConfig.silero_vad.min_silence_duration = static_cast<float>(m_config.audio.silenceTimeoutMs) / 1000;
    vadConfig.silero_vad.min_speech_duration = static_cast<float>(m_config.audio.minRecordMs) / 1000;
    vadConfig.silero_vad.max_speech_duration = static_cast<float>(m_config.audio.maxRecordMs) / 1000;
    vadConfig.sample_rate = m_config.audio.sampleRate;

    Diagnostics::logFileState("VAD | model", vadPath);
    std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> newVad;
    try {
        newVad = std::make_unique<sherpa_onnx::cxx::VoiceActivityDetector>(
            sherpa_onnx::cxx::VoiceActivityDetector::Create(vadConfig, static_cast<float>(m_config.audio.maxRecordMs) / 1000.0f + 5.0f));
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("VAD | create failed: %1").arg(e.what()));
        emit errorOccurred(tr("录音启动失败"), tr("VAD 模型加载失败，详见日志"));
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_vadMutex);
        m_vad = std::move(newVad);
    }

    constexpr int kPrePadMs = 150;
    constexpr int kPostPadMs = 150;
    int neededMs = kPrePadMs + qMax(kPostPadMs, m_config.audio.silenceTimeoutMs) + 300;
    m_historyCapacity = static_cast<int>(neededMs / 1000.0 * m_sampleRate);

    m_processedHistory.clear();
    m_historyStartSample = 0;
    m_totalSamplesFed = 0;
    m_agcGain = 1.0f;
    LOG_DEBUG(QString("VAD Model update successful (threshold=%1, silence=%2ms, sampleRate=%3)")
                  .arg(vadConfig.silero_vad.threshold).arg(m_config.audio.silenceTimeoutMs).arg(m_config.audio.sampleRate));
}


void VadWorker::processChunk(const QByteArray chunk)
{
    if (!m_vad) return;  
    const int16_t* samples = reinterpret_cast<const int16_t*>(chunk.constData());
    const int count = chunk.size() / 2;

    std::vector<float> floatSamples(count);
    for (int i = 0; i < count; ++i) {
        floatSamples[i] = samples[i] / 32768.0f;
    }

    m_vad->AcceptWaveform(floatSamples.data(), count);

    bool isSpeaking = m_vad->IsDetected();
    if (isSpeaking && !m_wasSpeaking) {
        emit speechStarted();
    }else if (!isSpeaking && m_wasSpeaking) {
        emit speechEnded();
    }
    m_wasSpeaking = isSpeaking;

    while (!m_vad->IsEmpty()) {
        auto segment = m_vad->Front();

        std::vector<int16_t> pcm16(segment.samples.size());
        for (size_t i = 0; i < segment.samples.size(); ++i) {
            float v = qBound(-1.0f, segment.samples[i], 1.0f);
            pcm16[i] = static_cast<int16_t>(v * 32767.0f);
        }
        QByteArray pcm(reinterpret_cast<const char*>(pcm16.data()),
            static_cast<int>(pcm16.size() * sizeof(int16_t)));
        emit speechSegmentReady(pcm, m_sampleRate);
        m_vad->Pop();
    }
}

void VadWorker::reset()
{
    m_vad->Reset();
    m_wasSpeaking = false;
}



AudioRecorderService::AudioRecorderService(const AppConfig& config, QObject *parent) : IRecorder(parent), m_config(config) {
    m_spectrumThread = new QThread(this); 
    m_spectrumWorker = new SpectrumWorker(m_config.audio.sampleRate); 
    m_spectrumWorker->moveToThread(m_spectrumThread); 
    connect(m_spectrumThread, &QThread::finished, m_spectrumWorker, &QObject::deleteLater); 
    connect(m_spectrumWorker, &SpectrumWorker::spectrumReady, this, &AudioRecorderService::spectrumUpdated);
    connect(m_spectrumWorker, &SpectrumWorker::levelUpdated, this, &AudioRecorderService::levelUpdated);
    m_spectrumThread->start();

    m_vadThread = new QThread(this);
    m_vadWorker = new VadWorker(config, m_config.audio.sampleRate);
    m_vadWorker->moveToThread(m_vadThread);
    connect(m_vadThread, &QThread::finished, m_vadWorker, &QObject::deleteLater);
    connect(m_vadWorker, &VadWorker::speechStarted, this, &AudioRecorderService::onVadSpeechStarted);
    connect(m_vadWorker, &VadWorker::speechEnded, this, &AudioRecorderService::onVadSpeechEnded);
    connect(m_vadWorker, &VadWorker::speechSegmentReady, this, &AudioRecorderService::onVadSegmentReady);
    connect(m_vadWorker, &VadWorker::errorOccurred, this, &AudioRecorderService::errorOccurred);

    m_vadThread->start();
}

AudioRecorderService::~AudioRecorderService() {
    if (m_audioSink) { m_audioSink->stop(); delete m_audioSink; m_audioSink = nullptr; }
    m_spectrumThread->quit();
    m_spectrumThread->wait();
    m_vadThread->quit();
    m_vadThread->wait();
}

QStringList AudioRecorderService::availableMicrophones() {
    QStringList names;
    names << AudioConfig::kDefaultDeviceName;
    for (const auto &dev : QMediaDevices::audioInputs())
        names << dev.description();
    return names;
}

QStringList AudioRecorderService::availableSpeakers() {
    QStringList names;
    names << AudioConfig::kDefaultDeviceName;
    for (const auto &dev : QMediaDevices::audioOutputs())
        names << dev.description();
    return names;
}


void AudioRecorderService::playTestTone() {
    QString outputDeviceName = m_config.audio.outputDeviceName;
    QAudioDevice m_outputDevice = QMediaDevices::defaultAudioOutput();
    if (!outputDeviceName.isEmpty() && outputDeviceName != AudioConfig::kDefaultDeviceName) {
        for (const auto& d : QMediaDevices::audioOutputs()) {
            if (d.description() == outputDeviceName) { m_outputDevice = d; break; }
        }
    }

    QAudioFormat fmt;
    fmt.setSampleRate(44100);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);

    // 清理上一次的 sink 与 buffer
    if (m_audioSink) { m_audioSink->stop(); delete m_audioSink; m_audioSink = nullptr; }
    if (m_audioBuffer) { delete m_audioBuffer; m_audioBuffer = nullptr; }

    m_audioSink = new QAudioSink(m_outputDevice, fmt, this);

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

void AudioRecorderService::updateConfig() {
    QMetaObject::invokeMethod(m_vadWorker, "rebuildDetector", Qt::QueuedConnection);
}

int AudioRecorderService::bytesPerMs() const
{
    return (m_config.audio.sampleRate * static_cast<int>(sizeof(int16_t))) / 1000;
}

QByteArray AudioRecorderService::normalizeChunk(const QByteArray& chunk) const
{
    const QAudioFormat& fmt = m_actualFormat;
    const int channels = std::max(1, fmt.channelCount());
    const int bytesPerSample = fmt.bytesPerSample();
    if (bytesPerSample <= 0) return {};

    const qsizetype frames = chunk.size() / (bytesPerSample * channels);
    if (frames == 0) return {};

    // 1) 解码并混成单声道 float
    std::vector<float> mono(static_cast<size_t>(frames));
    const char* raw = chunk.constData();
    for (qsizetype i = 0; i < frames; ++i) {
        float acc = 0.0f;
        for (int c = 0; c < channels; ++c) {
            const char* p = raw + (i * channels + c) * bytesPerSample;
            switch (fmt.sampleFormat()) {
            case QAudioFormat::UInt8:
                acc += (static_cast<uint8_t>(*p) - 128) / 128.0f;
                break;
            case QAudioFormat::Int16: {
                int16_t v; memcpy(&v, p, sizeof(v));
                acc += v / 32768.0f;
                break;
            }
            case QAudioFormat::Int32: {
                int32_t v; memcpy(&v, p, sizeof(v));
                acc += v / 2147483648.0f;
                break;
            }
            case QAudioFormat::Float: {
                float v; memcpy(&v, p, sizeof(v));
                acc += v;
                break;
            }
            default:
                break;
            }
        }
        mono[static_cast<size_t>(i)] = acc / channels;
    }

    // 2) 重采样到配置采样率。降采样时对窗口内取平均，相当于一个最简单的低通，
    //    避免高频折叠成噪声；升采样（少见）用线性插值。
    const int dstRate = m_config.audio.sampleRate;
    const double step = static_cast<double>(fmt.sampleRate()) / dstRate;
    const qsizetype outCount = static_cast<qsizetype>(frames / step);
    QByteArray out(static_cast<qsizetype>(outCount * sizeof(int16_t)), Qt::Uninitialized);
    int16_t* dst = reinterpret_cast<int16_t*>(out.data());

    for (qsizetype k = 0; k < outCount; ++k) {
        float v;
        if (step > 1.0) {
            const qsizetype begin = static_cast<qsizetype>(k * step);
            const qsizetype end = std::min<qsizetype>(frames, std::max<qsizetype>(begin + 1, static_cast<qsizetype>((k + 1) * step)));
            float sum = 0.0f;
            for (qsizetype i = begin; i < end; ++i) sum += mono[static_cast<size_t>(i)];
            v = sum / static_cast<float>(end - begin);
        } else {
            const double pos = k * step;
            const qsizetype i0 = static_cast<qsizetype>(pos);
            const qsizetype i1 = std::min<qsizetype>(i0 + 1, frames - 1);
            const float frac = static_cast<float>(pos - i0);
            v = mono[static_cast<size_t>(i0)] * (1.0f - frac) + mono[static_cast<size_t>(i1)] * frac;
        }
        dst[k] = static_cast<int16_t>(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
    }
    return out;
}

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

bool AudioRecorderService::startListening() {
    if (m_audioSource) return true;
    if (!ensureMicrophonePermission()) return false;

    QAudioFormat format;
    format.setSampleRate(m_config.audio.sampleRate);
    format.setChannelCount(m_config.audio.channels);
    format.setSampleFormat(m_config.audio.bitsPerSample == 8 ? QAudioFormat::UInt8 : QAudioFormat::Int16);

    QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (!m_config.audio.deviceName.isEmpty() && m_config.audio.deviceName != AudioConfig::kDefaultDeviceName) {
        for (const auto& d : QMediaDevices::audioInputs()) {
            if (d.description() == m_config.audio.deviceName) { device = d; break; }
        }
    }

    QString outputDeviceName = m_config.audio.outputDeviceName;
    QAudioDevice m_outputDevice = QMediaDevices::defaultAudioOutput();
    if (!outputDeviceName.isEmpty() && outputDeviceName != AudioConfig::kDefaultDeviceName) {
        for (const auto& d : QMediaDevices::audioOutputs()) {
            if (d.description() == outputDeviceName) { m_outputDevice = d; break; }
        }
    }

    if (!m_config.audio.outputDeviceName.isEmpty() && m_config.audio.outputDeviceName != AudioConfig::kDefaultDeviceName) {
        m_endpointController.setDefaultOutput(m_outputDevice.id().toStdString());
    }
    if (!m_config.audio.deviceName.isEmpty() && m_config.audio.deviceName != AudioConfig::kDefaultDeviceName) {
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
    // 缓冲区是设备侧的，必须按设备实际格式算，不能用下游的 bytesPerMs()
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

    auto _format = m_audioSource->format();
    m_actualFormat = _format;
    // 实际打开的设备格式（回退到 preferredFormat 或虚拟声卡的立体声）与下游期望的
    // 「配置采样率 / 单声道 / Int16」不一致时，在 onAudioDataReady 里统一转换。
    m_needsConversion = _format.sampleFormat() != QAudioFormat::Int16
                     || _format.channelCount() != 1
                     || _format.sampleRate() != m_config.audio.sampleRate;
    if (m_needsConversion) {
        LOG_WARN(QString("Recorder | 设备实际格式 %1Hz/%2ch/fmt=%3 与识别期望 %4Hz/1ch/Int16 不同，已启用格式转换")
                     .arg(_format.sampleRate()).arg(_format.channelCount())
                     .arg(static_cast<int>(_format.sampleFormat())).arg(m_config.audio.sampleRate));
    }
    LOG_DEBUG(QString("Sample Rate: %1").arg(_format.sampleRate()));
    LOG_DEBUG(QString("Channels: %1").arg(_format.channelCount()));
    LOG_DEBUG(QString("Sample Format: %1").arg(static_cast<int>(_format.sampleFormat())));

    connect(m_audioDevice, &QIODevice::readyRead, this, &AudioRecorderService::onAudioDataReady);
    m_status = RuntimeStatus{};
    m_status.isListening = true;
    m_segmentBuffer.clear();
    m_voiceActive = false;

    emit voiceStarted();
    return true;
}

void AudioRecorderService::stopListening() {
    if (!m_audioSource) return;
    m_voiceActive = false;

    // 恢复系统原来的默认播放/输入设备（若本会话切换过）。控制器内部判断是否需恢复。
    m_endpointController.restore();

    if (!m_config.continuousMode) {
        finalizeSegmentIfNeeded(true);
    }

    //if (!m_fullSessionBuffer.isEmpty()) {
    //    QString path = QApplication::applicationDirPath() + "/tmp";
    //    QDir dir(path);
    //    dir.mkdir(".");
    //    QString wavPath = path + "/debug_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".wav";
    //    if (writeWavFile(wavPath, m_fullSessionBuffer,
    //        m_config.audio.sampleRate,
    //        m_config.audio.channels,
    //        m_config.audio.bitsPerSample)) {
    //        qDebug() << "[debug] wav saved to" << path;
    //    }else {
    //        qDebug() << "[debug] wav save failed:" << path;
    //    }
    //}

    m_audioSource->stop();
    m_audioSource->deleteLater();

    m_audioSource = nullptr;
    m_audioDevice = nullptr;
    m_segmentBuffer.clear();
    m_status = RuntimeStatus{};
    QMetaObject::invokeMethod(m_spectrumWorker, "resetLevel", Qt::QueuedConnection);
}

void AudioRecorderService::pause() {
    if (!m_audioSource || m_status.isPaused) return;
    m_audioSource->suspend();  
    m_status.isPaused = true;
    QMetaObject::invokeMethod(m_spectrumWorker, "resetLevel", Qt::QueuedConnection);
}

void AudioRecorderService::resume() {
    if (!m_audioSource || !m_status.isPaused) return;
    m_audioSource->resume();
    m_status.isPaused = false;
}

void AudioRecorderService::onAudioDataReady() {
    if (!m_audioDevice) return;
    QByteArray chunk = m_audioDevice->readAll();
    if (chunk.isEmpty()) return;
    if (m_needsConversion) {
        chunk = normalizeChunk(chunk);
        if (chunk.isEmpty()) return;
    }

    QMetaObject::invokeMethod(m_spectrumWorker, "processChunk", Qt::QueuedConnection,
        Q_ARG(QByteArray, chunk));

    if (!m_config.continuousMode.load()) {
        m_segmentBuffer.append(chunk);
        m_status.currentSegmentMs = static_cast<int>(m_segmentBuffer.size() / bytesPerMs());
    }else {
        QMetaObject::invokeMethod(m_vadWorker, "processChunk", Qt::QueuedConnection,
            Q_ARG(QByteArray, chunk));
    }
}

void AudioRecorderService::onVadSpeechStarted()
{
    m_voiceActive = true;
    emit voiceStarted();   
}

void AudioRecorderService::onVadSpeechEnded()
{
    m_voiceActive = false;
    emit voiceStopped();   
}

void AudioRecorderService::onVadSegmentReady(const QByteArray& pcmData, int sampleRate)
{
    if (m_config.continuousMode.load()) {
        if (!pcmData.isEmpty()) {
            emit utteranceReady(pcmData, sampleRate);
        }
    }
}

void AudioRecorderService::finalizeSegmentIfNeeded(bool forceCut)
{
    if (m_segmentBuffer.isEmpty()) return;

    if (m_status.currentSegmentMs < m_config.audio.minRecordMs && !forceCut) {
        m_segmentBuffer.clear();
        m_status.currentSegmentMs = 0;
        emit voiceStopped();
        return;
    }

    emit utteranceReady(m_segmentBuffer, m_config.audio.sampleRate);
    emit voiceStopped();

    m_segmentBuffer.clear();
    m_status.currentSegmentMs = 0;
}

bool AudioRecorderService::isListening() const { return m_status.isListening; }
bool AudioRecorderService::isPaused() const { return m_status.isPaused; }
AudioRecorderService::RuntimeStatus AudioRecorderService::runtimeStatus() const { return m_status; }
bool AudioRecorderService::isVoiceActive() const { return m_voiceActive.load(); }

bool AudioRecorderService::writeWavFile(const QString& filePath, const QByteArray& pcmData,
    int sampleRate, int channels, int bitsPerSample) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    const quint32 dataSize = static_cast<quint32>(pcmData.size());
    const quint32 byteRate = sampleRate * channels * (bitsPerSample / 8);
    const quint16 blockAlign = static_cast<quint16>(channels * (bitsPerSample / 8));
    const quint32 riffSize = 36 + dataSize;

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    // RIFF header
    out.writeRawData("RIFF", 4);
    out << riffSize;
    out.writeRawData("WAVE", 4);

    // fmt chunk
    out.writeRawData("fmt ", 4);
    out << static_cast<quint32>(16);              // fmt chunk size (PCM)
    out << static_cast<quint16>(1);                // audio format = 1 (PCM)
    out << static_cast<quint16>(channels);
    out << static_cast<quint32>(sampleRate);
    out << byteRate;
    out << blockAlign;
    out << static_cast<quint16>(bitsPerSample);

    // data chunk
    out.writeRawData("data", 4);
    out << dataSize;
    out.writeRawData(pcmData.constData(), pcmData.size());

    file.close();
    return true;
}