#pragma once

#include <QByteArray>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>

#include "audio/AudioFileDecoder.h"
#include "config/AppConfig.h"
#include "ui/format/SubtitleFormat.h"

class QAudioDecoder;
class QNetworkAccessManager;
class TranscriptionService;
class SherpaManager;
class YtDlpManager;

class FileTranscriptionPipeline : public QObject {
    Q_OBJECT
public:
    enum class State {
        Idle,
        Decoding,
        Segmenting,
        Transcribing,
        Completed,
        Failed,
        NoSpeech,
        Cancelled
    };

    enum class Outcome {
        Done,
        Failed,
        NoSpeech,
        Cancelled
    };

    struct Job {
        int row = -1;
        quint64 runToken = 0;
        QString path;
        AppConfig config;
    };

    explicit FileTranscriptionPipeline(QNetworkAccessManager* networkManager,
                                       TranscriptionService* transcription,
                                       SherpaManager* sherpa,
                                       QObject* parent = nullptr);

    static bool isDownloadSupported();
    quint64 enqueueDownload(const QString& url);
    void cancelDownload(quint64 id);
    void cancelDownloads();
    void releaseDownload(quint64 id);

    bool start(const Job& job);
    void cancel();

signals:
    void downloadToolStatus(const QString& text);
    void downloadProgress(quint64 id, int percent);
    void downloadTitle(quint64 id, const QString& title);
    void downloadFinished(quint64 id, bool ok, const QString& path,
                          const QString& title, const QString& error);

    void stateChanged(int row, FileTranscriptionPipeline::State state, int done, int total);
    void segmentReady(int row, const SubtitleCue& cue, const QString& text);
    void translationReady(quint64 runToken, int cue, const QString& language, const QString& text);
    void translationFailed(quint64 runToken, const QString& cause);
    void fileFinished(int row, FileTranscriptionPipeline::Outcome outcome, const QString& error);

private:
    struct Segment {
        QByteArray pcm;
        double startSec = 0;
        double endSec = 0;
    };

    struct SegmentationResult {
        QList<Segment> segments;
        QString error;
    };

    void startDecode();
    void startQtDecode();
    void readQtDecodeBuffer();
    void finishQtDecode();
    void onFileDecoded();
    void startSegmenting();
    void onSegmentsReady();
    void transcribeNext();
    void onSegmentFinished(bool ok, const QString& rawText,
                           const QString& finalText, const QString& error);
    void requestTranslation(int cue, const QString& text);
    void finish(Outcome outcome, const QString& error = {});
    void setState(State state, int done = 0, int total = 0);

    static SegmentationResult segmentByVad(const QByteArray& pcm, const AppConfig& config);

    TranscriptionService* m_transcription = nullptr;
    SherpaManager* m_sherpa = nullptr;
    YtDlpManager* m_downloader = nullptr;
    QAudioDecoder* m_decoder = nullptr;
    QFutureWatcher<AudioFileDecoder::Result> m_decodeWatcher;
    QFutureWatcher<SegmentationResult> m_segmentationWatcher;
    std::shared_ptr<std::atomic_bool> m_decodeCancel;
    Job m_job;
    QByteArray m_pcm;
    QList<Segment> m_segments;
    int m_nextSegment = 0;
    int m_cueCount = 0;
    bool m_active = false;
    bool m_cancelled = false;
    bool m_waitingSegment = false;
};

Q_DECLARE_METATYPE(FileTranscriptionPipeline::State)
Q_DECLARE_METATYPE(FileTranscriptionPipeline::Outcome)
