#include "RecorderService.h"
#include <QMediaDevices>
#include <QAudioDevice>
#include <QFile>
#include <QDateTime>
#include <QDataStream>
#include <QApplication>
#include <QDir>



AudioRecorderService::AudioRecorderService(const AppConfig& config, QObject *parent) : QObject(parent), m_config(config) {
    m_kissFftCfg = kiss_fftr_alloc(kFftSize, 0, nullptr, nullptr);
}

AudioRecorderService::~AudioRecorderService() { 
    if (m_kissFftCfg) {
        kiss_fftr_free(static_cast<kiss_fftr_cfg>(m_kissFftCfg));
    }
}

QStringList AudioRecorderService::availableMicrophones() {
    QStringList names;
    names << "系统默认录音设备";
    for (const auto &dev : QMediaDevices::audioInputs())
        names << dev.description();
    return names;
}

int AudioRecorderService::bytesPerMs() const
{
    return (m_config.audio.sampleRate * m_config.audio.channels * (m_config.audio.bitsPerSample / 8)) / 1000;
}

bool AudioRecorderService::startListening() {
    if (m_audioSource) return true;

    QAudioFormat format;
    format.setSampleRate(m_config.audio.sampleRate);
    format.setChannelCount(m_config.audio.channels);
    format.setSampleFormat(m_config.audio.bitsPerSample == 8 ? QAudioFormat::UInt8 : QAudioFormat::Int16);

    QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (!m_config.audio.deviceName.isEmpty() && m_config.audio.deviceName != "系统默认录音设备") {
        for (const auto& d : QMediaDevices::audioInputs()) {
            if (d.description() == m_config.audio.deviceName) { device = d; break; }
        }
    }

    if (!device.isFormatSupported(format)) {
        qWarning() << "Default format not supported, trying to use the nearest.";
        format = device.preferredFormat();
    }

    m_audioSource = new QAudioSource(device, format, this);
    m_audioDevice = m_audioSource->start();
    if (!m_audioDevice) return false;


    auto _format = m_audioSource->format();
    qDebug() << "Sample Rate: " << _format.sampleRate();
    qDebug() << "Channels: " << _format.channelCount();
    qDebug() << "Sample Format: " << _format.sampleFormat();

    connect(m_audioDevice, &QIODevice::readyRead, this, &AudioRecorderService::onAudioDataReady);

    m_status = RuntimeStatus{};
    m_status.isListening = true;
    m_segmentBuffer.clear();
    m_silenceAccumMs = 0;
    m_fullSessionBuffer.clear();
    m_manualActive = true;

    if (!m_config.continuousMode) {
        m_status.hadVoice = true;
        emit voiceStarted();
    }
    return true;
}

void AudioRecorderService::stopListening() {
    if (!m_audioSource) return;

    if (!m_config.continuousMode) {
        finalizeSegmentIfNeeded(true);
    }

    if (!m_fullSessionBuffer.isEmpty()) {
        QString path = QApplication::applicationDirPath() + "/tmp";

        QDir dir(path);
        dir.mkdir(".");

        QString wavPath = path + "/debug_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".wav";
        if (writeWavFile(wavPath, m_fullSessionBuffer,
            m_config.audio.sampleRate,
            m_config.audio.channels,
            m_config.audio.bitsPerSample)) {
            qDebug() << "[debug] wav saved to" << path;
        }
        else {
            qDebug() << "[debug] wav save failed:" << path;
        }
    }

    m_audioSource->stop();
    m_audioSource->deleteLater();
    m_audioSource = nullptr;
    m_audioDevice = nullptr;
    m_segmentBuffer.clear();
    m_fullSessionBuffer.clear();
    m_status = RuntimeStatus{};
    emit levelUpdated(0.0f);
}

void AudioRecorderService::pause() {
    if (!m_audioSource || m_status.isPaused) return;
    m_audioSource->suspend();  
    m_status.isPaused = true;
    m_status.rmsLevel = 0.0f;
    emit levelUpdated(0.0f);
}

void AudioRecorderService::resume() {
    if (!m_audioSource || !m_status.isPaused) return;
    m_audioSource->resume();
    m_status.isPaused = false;
    m_silenceAccumMs = 0;
}


// TODO: 需要进行优化，添加VAD，通过VAD来判断切割
void AudioRecorderService::onAudioDataReady() {
    if (!m_audioDevice) return;
    QByteArray chunk = m_audioDevice->readAll();
    if (chunk.isEmpty()) return;

    m_fullSessionBuffer.append(chunk);
    updateVadState(chunk);
    //qDebug() << "RMS Level: " << m_status.rmsLevel;
    emit levelUpdated(m_status.rmsLevel);
    updateSpectrum(chunk);

    if (m_config.continuousMode) {
        if (m_status.hadVoice) {
            m_segmentBuffer.append(chunk);
            m_status.currentSegmentMs = static_cast<int>(m_segmentBuffer.size() / bytesPerMs());
        }

        // 强制切断:一句话讲太长,防止用户长时间不停顿导致缓冲区无限增长
        if (m_status.hadVoice && m_status.currentSegmentMs >= m_config.audio.maxRecordMs) {
            finalizeSegmentIfNeeded(true);
            return;
        }

        int chunkMs = static_cast<int>(chunk.size() / bytesPerMs());
        if (m_status.peakLevel < m_config.audio.voiceThreshold) {
            m_silenceAccumMs += chunkMs;
        }
        else {
            m_silenceAccumMs = 0;
        }

        if (m_status.hadVoice && m_silenceAccumMs >= m_config.audio.silenceTimeoutMs) {
            finalizeSegmentIfNeeded(false);
        }
	}
	else {  
        if (m_manualActive) {
            m_segmentBuffer.append(chunk);
            m_status.currentSegmentMs = static_cast<int>(m_segmentBuffer.size() / bytesPerMs());

            if (m_status.currentSegmentMs >= m_config.audio.maxRecordMs) {
                finalizeSegmentIfNeeded(true);
                m_manualActive = false;
            }
        }
    }
}


void AudioRecorderService::finalizeSegmentIfNeeded(bool forceCut)
{
    if (m_segmentBuffer.isEmpty()) return;

    if (m_status.currentSegmentMs < m_config.audio.minRecordMs && !forceCut) {
        m_segmentBuffer.clear();
        m_status.hadVoice = false;
        m_silenceAccumMs = 0;
        m_status.currentSegmentMs = 0;
        emit voiceStopped();
        return;
    }

    emit utteranceReady(m_segmentBuffer, m_config.audio.sampleRate);
    emit voiceStopped();

    m_segmentBuffer.clear();
    m_status.hadVoice = false;
    m_silenceAccumMs = 0;
    m_status.currentSegmentMs = 0;
}


void AudioRecorderService::updateVadState(const QByteArray &chunk) {
    const int16_t* samples = reinterpret_cast<const int16_t*>(chunk.constData());
    const int count = chunk.size() / 2;
    if (count == 0) return;

    int peak = 0;
    double sumSquares = 0.0;
    for (int i = 0; i < count; ++i) {
        int v = static_cast<int>(samples[i]);
        peak = qMax(peak, qAbs(v));
        sumSquares += static_cast<double>(v) * v;
    }

    double rms = std::sqrt(sumSquares / count);
    m_status.rmsLevel = static_cast<float>(qMin(1.0, rms / 12000.0));
    m_status.peakLevel = peak;

    if (peak >= m_config.audio.voiceThreshold && !m_status.hadVoice) {
        m_status.hadVoice = true;
        emit voiceStarted();
    }
}

bool AudioRecorderService::isListening() const { return m_status.isListening; }
bool AudioRecorderService::isPaused() const { return m_status.isPaused; }
AudioRecorderService::RuntimeStatus AudioRecorderService::runtimeStatus() const { return m_status; }



void AudioRecorderService::updateSpectrum(const QByteArray& chunk)
{
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

    const double sampleRate = m_config.audio.sampleRate; 
    const double binHz = sampleRate / kFftSize;         

    const double voiceMinHz = 80.0;   
    const double voiceMaxHz = 1000.0; 

    int minBin = qMax(1, static_cast<int>(voiceMinHz / binHz));      
    int maxBin = qMin(static_cast<int>(magnitudes.size()) - 1,
        static_cast<int>(voiceMaxHz / binHz));
    if (maxBin <= minBin) maxBin = minBin + 1; 

    QVector<float> bands(kBandCount, 0.0f);
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

        float peak = 0.0f; 
        for (int i = lowBin; i < highBin && i < static_cast<int>(magnitudes.size()); ++i) {
            peak = qMax(peak, magnitudes[i]);
        }
        bands[b] = peak;
    }

    for (int b = 0; b < kBandCount; ++b) {
        float mag = bands[b];
        float db = 20.0f * std::log10(mag + 1.0f);

        const float dbMin = 0.0f;
        const float dbMax = 45.0f;

        float normalized = (db - dbMin) / (dbMax - dbMin);
        bands[b] = qBound(0.0f, normalized, 1.0f);
    }

    QVector<float> result(bands.begin(), bands.end());
    emit spectrumUpdated(bands);
}

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