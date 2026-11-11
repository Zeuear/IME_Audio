#include "FileTranscriptionPipeline.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QFile>
#include <QPointer>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>
#include <utility>
#include <vector>

#include "services/TranscriptionService.h"
#include "services/media/YtDlpManager.h"
#include "sherpa/SherpaManager.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"
#include "translation/LanguageRouting.h"
#include "cxx-api.h"

namespace {
constexpr int kSampleRate = 16000;
constexpr float kMaxSegmentSec = 30.0f;
}

FileTranscriptionPipeline::FileTranscriptionPipeline(QNetworkAccessManager* networkManager,
                                                     TranscriptionService* transcription,
                                                     SherpaManager* sherpa,
                                                     QObject* parent)
    : QObject(parent)
    , m_transcription(transcription)
    , m_sherpa(sherpa)
    , m_downloader(new YtDlpManager(networkManager, this))
{
    connect(m_downloader, &YtDlpManager::toolStatus, this, &FileTranscriptionPipeline::downloadToolStatus);
    connect(m_downloader, &YtDlpManager::taskTitle, this, &FileTranscriptionPipeline::downloadTitle);
    connect(m_downloader, &YtDlpManager::taskFinished, this,
            [this](quint64 id, bool ok, const QString& path, const QString& title, const QString& error) {
                emit downloadFinished(id, ok, path, title, error);
            });
    connect(m_downloader, &YtDlpManager::taskProgress, this, &FileTranscriptionPipeline::downloadProgress);

    connect(&m_decodeWatcher, &QFutureWatcher<AudioFileDecoder::Result>::finished,
            this, &FileTranscriptionPipeline::onFileDecoded);
    connect(&m_segmentationWatcher, &QFutureWatcher<SegmentationResult>::finished,
            this, &FileTranscriptionPipeline::onSegmentsReady);
    connect(m_transcription, &TranscriptionService::fileSegmentFinished,
            this, &FileTranscriptionPipeline::onSegmentFinished);
}

quint64 FileTranscriptionPipeline::enqueueDownload(const QString& url)
{
    return m_downloader->enqueue(url);
}

bool FileTranscriptionPipeline::isDownloadSupported()
{
    return YtDlpManager::isPlatformSupported();
}

void FileTranscriptionPipeline::cancelDownload(quint64 id)
{
    m_downloader->cancel(id);
}

void FileTranscriptionPipeline::cancelDownloads()
{
    m_downloader->cancelAll();
}

void FileTranscriptionPipeline::releaseDownload(quint64 id)
{
    m_downloader->release(id);
}

bool FileTranscriptionPipeline::start(const Job& job)
{
    if (m_active || job.row < 0 || job.path.isEmpty()) return false;

    m_job = job;
    m_active = true;
    m_cancelled = false;
    m_waitingSegment = false;
    m_pcm.clear();
    m_segments.clear();
    m_nextSegment = 0;
    m_cueCount = 0;
    setState(State::Decoding);
    startDecode();
    return true;
}

void FileTranscriptionPipeline::startDecode()
{
    if (!AudioFileDecoder::isAvailable()) {
        startQtDecode();
        return;
    }

    m_decodeCancel = std::make_shared<std::atomic_bool>(false);
    const QString path = m_job.path;
    const auto cancelFlag = m_decodeCancel;
    m_decodeWatcher.setFuture(QtConcurrent::run([path, cancelFlag]() {
        return AudioFileDecoder::decode(path, kSampleRate, cancelFlag.get());
    }));
}

void FileTranscriptionPipeline::startQtDecode()
{
    if (m_decoder) {
        m_decoder->disconnect(this);
        m_decoder->stop();
        m_decoder->deleteLater();
    }
    m_decoder = new QAudioDecoder(this);
    connect(m_decoder, &QAudioDecoder::bufferReady,
            this, &FileTranscriptionPipeline::readQtDecodeBuffer);
    connect(m_decoder, &QAudioDecoder::finished,
            this, &FileTranscriptionPipeline::finishQtDecode);
    connect(m_decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this,
            [this](QAudioDecoder::Error) {
                if (m_active && !m_cancelled) finish(Outcome::Failed, m_decoder->errorString());
            });

    QAudioFormat format;
    format.setSampleRate(kSampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    m_decoder->setAudioFormat(format);
    m_decoder->setSource(QUrl::fromLocalFile(m_job.path));
    m_decoder->start();
}

void FileTranscriptionPipeline::readQtDecodeBuffer()
{
    while (m_decoder && m_decoder->bufferAvailable()) {
        const QAudioBuffer buffer = m_decoder->read();
        if (!buffer.isValid()) break;
        m_pcm.append(buffer.constData<char>(), buffer.byteCount());
    }
}

void FileTranscriptionPipeline::finishQtDecode()
{
    if (!m_active) return;
    readQtDecodeBuffer();
    if (m_cancelled) {
        finish(Outcome::Cancelled);
    } else if (m_pcm.isEmpty()) {
        LOG_WARN(QString("Decoder finished with no data: error=%1 '%2' duration=%3ms source=%4")
                     .arg(int(m_decoder->error())).arg(m_decoder->errorString())
                     .arg(m_decoder->duration()).arg(m_decoder->source().toString()));
        finish(Outcome::Failed, tr("No audio data decoded"));
    } else {
        startSegmenting();
    }
}

void FileTranscriptionPipeline::onFileDecoded()
{
    if (!m_active) return;
    const AudioFileDecoder::Result result = m_decodeWatcher.result();
    if (m_cancelled) {
        finish(Outcome::Cancelled);
    } else if (!result.error.isEmpty()) {
        finish(Outcome::Failed, result.error);
    } else {
        m_pcm = result.pcm;
        startSegmenting();
    }
}

void FileTranscriptionPipeline::startSegmenting()
{
    setState(State::Segmenting);
    const QByteArray pcm = m_pcm;
    const AppConfig config = m_job.config;
    m_segmentationWatcher.setFuture(QtConcurrent::run([pcm, config]() {
        return segmentByVad(pcm, config);
    }));
}

FileTranscriptionPipeline::SegmentationResult
FileTranscriptionPipeline::segmentByVad(const QByteArray& pcm, const AppConfig& config)
{
    SegmentationResult result;
    QString vadPath = config.sherpa.vadPath;
    if (!QFile::exists(vadPath)) vadPath = AppPaths::vadModelFile();
    if (!QFile::exists(vadPath)) {
        result.error = QStringLiteral("VAD 模型缺失，请先下载模型");
        return result;
    }

    try {
        sherpa_onnx::cxx::VadModelConfig vadConfig;
        vadConfig.silero_vad.model = vadPath.toStdString();
        vadConfig.silero_vad.threshold = static_cast<float>(config.audio.voiceThreshold) / 1000;
        vadConfig.silero_vad.min_silence_duration = 0.5f;
        vadConfig.silero_vad.min_speech_duration = 0.25f;
        vadConfig.silero_vad.max_speech_duration = kMaxSegmentSec;
        vadConfig.sample_rate = kSampleRate;

        auto vad = sherpa_onnx::cxx::VoiceActivityDetector::Create(vadConfig, 60.0f);
        std::vector<std::pair<int, int>> spans;
        auto drain = [&]() {
            while (!vad.IsEmpty()) {
                const auto segment = vad.Front();
                spans.emplace_back(segment.start, segment.start + static_cast<int>(segment.samples.size()));
                vad.Pop();
            }
        };

        const auto* samples = reinterpret_cast<const int16_t*>(pcm.constData());
        const int sampleCount = pcm.size() / 2;
        constexpr int kChunkSamples = 512 * 8;
        std::vector<float> buffer(kChunkSamples);
        for (int position = 0; position < sampleCount; position += kChunkSamples) {
            const int count = qMin(kChunkSamples, sampleCount - position);
            for (int i = 0; i < count; ++i) buffer[i] = samples[position + i] / 32768.0f;
            vad.AcceptWaveform(buffer.data(), count);
            drain();
        }
        vad.Flush();
        drain();

        constexpr int kMergeGapSamples = kSampleRate;
        constexpr int kMergeMaxSamples = kSampleRate * 10;
        std::vector<std::pair<int, int>> merged;
        for (const auto& span : spans) {
            if (!merged.empty()
                && span.first - merged.back().second < kMergeGapSamples
                && span.second - merged.back().first <= kMergeMaxSamples) {
                merged.back().second = span.second;
            } else {
                merged.push_back(span);
            }
        }

        constexpr int kEdgePaddingSamples = kSampleRate * 3 / 10;
        for (size_t i = 0; i < merged.size(); ++i) {
            const int lowerBound = i > 0 ? merged[i - 1].second : 0;
            const int upperBound = i + 1 < merged.size() ? merged[i + 1].first : sampleCount;
            const int begin = qMax(lowerBound, merged[i].first - kEdgePaddingSamples);
            const int end = qMin(upperBound, merged[i].second + kEdgePaddingSamples);
            result.segments.append({
                pcm.mid(begin * 2, (end - begin) * 2),
                static_cast<double>(merged[i].first) / kSampleRate,
                static_cast<double>(merged[i].second) / kSampleRate
            });
        }
    } catch (const std::exception& e) {
        result.error = QString::fromLocal8Bit(e.what());
    }

    return result;
}

void FileTranscriptionPipeline::onSegmentsReady()
{
    if (!m_active) return;
    if (m_cancelled) {
        finish(Outcome::Cancelled);
        return;
    }

    const SegmentationResult result = m_segmentationWatcher.result();
    if (!result.error.isEmpty()) {
        finish(Outcome::Failed, result.error);
        return;
    }

    m_segments = result.segments;
    m_pcm.clear();
    if (m_segments.isEmpty()) {
        finish(Outcome::NoSpeech);
        return;
    }

    if (m_job.config.backend == AsrBackendKind::Sherpa) {
        if (!m_sherpa) {
            finish(Outcome::Failed, QStringLiteral("Sherpa 转录服务未配置"));
            return;
        }
        m_sherpa->pauseIdleTimer();
        if (!m_sherpa->isModelLoaded()) m_sherpa->loadModelAsync(m_job.config, false);
    }
    setState(State::Transcribing, 0, m_segments.size());
    transcribeNext();
}

void FileTranscriptionPipeline::transcribeNext()
{
    if (m_cancelled) {
        finish(Outcome::Cancelled);
        return;
    }
    if (m_nextSegment >= m_segments.size()) {
        finish(m_cueCount == 0 ? Outcome::NoSpeech : Outcome::Done);
        return;
    }

    m_waitingSegment = true;
    m_transcription->transcribe(m_segments.at(m_nextSegment).pcm, kSampleRate, 1, 16);
}

void FileTranscriptionPipeline::onSegmentFinished(bool ok, const QString&,
                                                   const QString& finalText, const QString& error)
{
    if (!m_active || !m_waitingSegment) return;
    m_waitingSegment = false;
    if (m_cancelled) {
        finish(Outcome::Cancelled);
        return;
    }
    if (!ok) {
        finish(Outcome::Failed, error);
        return;
    }

    const QString text = finalText.trimmed();
    if (!text.isEmpty()) {
        const Segment& segment = m_segments.at(m_nextSegment);
        const SubtitleCue cue{qRound64(segment.startSec * 1000),
                              qRound64(segment.endSec * 1000), text};
        emit segmentReady(m_job.row, cue, text);
        requestTranslation(m_cueCount++, text);
    }

    ++m_nextSegment;
    setState(State::Transcribing, m_nextSegment, m_segments.size());
    transcribeNext();
}

void FileTranscriptionPipeline::requestTranslation(int cue, const QString& text)
{
    if (!m_transcription->fileTranslationActive()) return;

    const quint64 runToken = m_job.runToken;
    const QString language = languageCodeForStored(m_transcription->fileTranslationTarget());
    QPointer<FileTranscriptionPipeline> self(this);
    m_transcription->translateForFile(text, [self, runToken, cue, language](
                                                bool ok, const QString& translated, const QString& error) {
        if (!self) return;
        if (ok) {
            emit self->translationReady(runToken, cue, language, translated.trimmed());
        } else {
            emit self->translationFailed(runToken, error);
        }
    });
}

void FileTranscriptionPipeline::cancel()
{
    if (!m_active || m_cancelled) return;
    m_cancelled = true;
    if (m_decodeCancel) *m_decodeCancel = true;

    if (m_decoder && m_decoder->isDecoding()) {
        m_decoder->stop();
        finish(Outcome::Cancelled);
    } else if (!m_decodeWatcher.isRunning() && !m_segmentationWatcher.isRunning() && !m_waitingSegment) {
        finish(Outcome::Cancelled);
    }
}

void FileTranscriptionPipeline::finish(Outcome outcome, const QString& error)
{
    if (!m_active) return;
    State finalState = State::Cancelled;
    switch (outcome) {
    case Outcome::Done: finalState = State::Completed; break;
    case Outcome::Failed: finalState = State::Failed; break;
    case Outcome::NoSpeech: finalState = State::NoSpeech; break;
    case Outcome::Cancelled: break;
    }
    setState(finalState, m_nextSegment, m_segments.size());
    m_active = false;
    if (m_decoder && m_decoder->isDecoding()) m_decoder->stop();
    if (m_decodeCancel) *m_decodeCancel = true;
    m_waitingSegment = false;
    m_pcm.clear();
    m_segments.clear();
    emit fileFinished(m_job.row, outcome, error);
}

void FileTranscriptionPipeline::setState(State state, int done, int total)
{
    emit stateChanged(m_job.row, state, done, total);
}
