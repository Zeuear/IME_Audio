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
class SeparationModelManager;

// @brief 协调文件下载、解码、分段转录和翻译。
class FileTranscriptionPipeline : public QObject {
    Q_OBJECT
public:
    enum class State {
        Idle,
        Decoding,
        DownloadingModel,   // done = 下载百分比，total = 100
        Separating,         // done / total = 已处理块数 / 总块数
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
        bool separateVocals = false;   // 先分离人声，再转录人声
    };

    explicit FileTranscriptionPipeline(QNetworkAccessManager* networkManager,
                                       TranscriptionService* transcription,
                                       SherpaManager* sherpa,
                                       QObject* parent = nullptr);
    ~FileTranscriptionPipeline() override;

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
    // 人声分离完成：path 是临时 WAV，所有权交给界面（用完需删除）
    void vocalsReady(int row, const QString& path);
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
    void startSeparateDecode();
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
    SeparationModelManager* m_separationModel = nullptr;
    QString m_vocalsPath;   // 本次分离输出；非空表示解码结果来自人声
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
