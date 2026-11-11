#pragma once
#include <QWidget>
#include <QByteArray>
#include <QList>
#include <QFutureWatcher>
#include <atomic>
#include <memory>
#include "ui/models/FileTaskModel.h"
#include "audio/AudioFileDecoder.h"

class QAudioDecoder;
class QNetworkAccessManager;
class AnimatedCheckBox;
class YtDlpManager;
class QLabel;
class QListView;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTimer;
class TranscriptionService;
class SherpaManager;
struct AppConfig;

// @brief
// 文件转录页：上半部分是文件卡片列表（拖入/选择添加），下半部分预览所选文件的转录文本。
// 转录流程：解码为 16k 单声道 PCM（有 FFmpeg 时用 AudioFileDecoder，否则 QAudioDecoder）
// → VAD 切段 → 逐段送入转录后端，按队列依次处理各文件。
// 转录期间 TranscriptionService 处于文件模式，结果不会走听写工作流（不会被注入前台窗口）。
// 开启了翻译设置时，每段原文另外翻译，译文显示在预览右侧，可单独复制和导出。
class FileTranscribeWidget : public QWidget {
    Q_OBJECT
public:
    explicit FileTranscribeWidget(QWidget* parent = nullptr);

    void setServices(TranscriptionService* transcription, SherpaManager* sherpa, const AppConfig* config);

    bool isBusy() const { return m_busy; }

signals:
    void errorOccurred(const QString& title, const QString& cause = {});
    // 批量转录开始 / 结束；横条据此在悬停提示里注明"转录中"
    void busyChanged(bool busy);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct Segment {
        QByteArray pcm;
        double startSec = 0;   // 相对原音频开头
        double endSec = 0;
    };
    using Segments = QList<Segment>;

    void setupUi();
    void retranslateUi();

    // 列表与按钮
    void chooseFiles();
    void addFiles(const QStringList& paths);
    QList<int> selectedRows() const;

    // 链接
    void addLinksClicked();
    void startDownloads(const QStringList& urls);
    void ensureDownloader();
    void enqueueDownload(int row);
    void onLinkFinished(quint64 id, bool ok, const QString& path, const QString& title, const QString& error);
    void queueForTranscription(int row);
    // 筛出能直接转录的行；下载失败的链接任务在这里改为重新下载
    QList<int> runnableRows(const QList<int>& candidates);
    void updateAnimTimer();
    static QString exportBaseName(const FileTask& task);
    void removeSelected();
    void clearAll();
    void showContextMenu(const QPoint& pos);
    void updatePreview();
    void updateTranslationPreview(const FileTask* task, bool sameFile);
    void updateButtons();
    void setBusy(bool busy);

    // 转录批次
    void transcribeClicked();
    void startRun(const QList<int>& rows);
    void startNextFile();
    void collectAppendedRows();
    void cancel();
    void finishFile(FileTask::State state, const QString& error = {});
    void fail(const QString& cause);
    void finishAll();

    void startQtDecode();
    void onDecodeBuffer();
    void onDecodeFinished();
    void onFileDecoded();
    void startSegmenting();
    void onSegmentsReady();
    void transcribeNext();
    void onSegmentFinished(bool ok, const QString& rawText, const QString& finalText, const QString& err);

    // 译文：每段原文出来后异步翻译，按段下标填回，不阻塞转录
    void requestTranslation(int row, const QString& text);
    void onTranslationFailed(const QString& cause);

    // 导出
    void exportResults();
    void copyPreview();
    void copyTranslation();
    static bool writeTextFile(const QString& path, const QString& text);
    static QString uniqueExportBase(const QString& dir, const QString& base, const QStringList& exts);

    static Segments segmentByVad(const QByteArray& pcm, const AppConfig& config, QString* error);

    TranscriptionService* m_transcription = nullptr;
    SherpaManager* m_sherpa = nullptr;
    const AppConfig* m_config = nullptr;

    QAudioDecoder* m_decoder = nullptr;
    QFutureWatcher<AudioFileDecoder::Result>* m_fileDecodeWatcher = nullptr;
    // 每个文件一份：后台解码线程持有共享所有权，控件先销毁也不会读到悬空的标志
    std::shared_ptr<std::atomic_bool> m_decodeCancel;
    QFutureWatcher<Segments>* m_vadWatcher = nullptr;
    QString m_vadError;

    FileTaskModel* m_model = nullptr;
    QStackedWidget* m_listStack = nullptr;
    QListView* m_list = nullptr;
    QLabel* m_hintLabel = nullptr;
    QLabel* m_previewTitle = nullptr;
    QPlainTextEdit* m_preview = nullptr;
    QPlainTextEdit* m_translationPreview = nullptr;
    QString m_previewPath;   // 预览当前显示的文件，用来区分"同一文件内容更新"和"换了文件"
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_addLinkBtn = nullptr;
    QPushButton* m_transcribeBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    AnimatedCheckBox* m_exportTxtCheck = nullptr;
    AnimatedCheckBox* m_exportSrtCheck = nullptr;
    AnimatedCheckBox* m_exportTranslationCheck = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_copyTranslationBtn = nullptr;
    QPushButton* m_openFolderBtn = nullptr;
    QTimer* m_animTimer = nullptr;   // 驱动"解码中"不确定进度条的重绘

    // 批次状态
    QList<int> m_runRows;      // 本批待处理的行（行号在批次内稳定：转录中禁止删除）
    int m_runWatermark = 0;    // 批次开始时的行数；之后追加的行在队列清空时并入本批
    int m_currentRow = -1;
    int m_fileTotal = 0;
    int m_fileDone = 0;
    int m_okCount = 0;
    int m_failedCount = 0;
    int m_noSpeechCount = 0;
    QString m_lastError;
    QString m_lastExportDir;
    bool m_translationErrorShown = false;   // 同一批次翻译失败只弹一次，避免逐句弹窗
    // 翻译设置是否生效的缓存：预览刷新很频繁，只在页面显示和批次开始时重新查询
    bool m_translationActive = false;

    QNetworkAccessManager* m_nam = nullptr;
    YtDlpManager* m_downloader = nullptr;
    int m_linkFailed = 0;          // 本轮下载失败的个数，全部下完时汇总提示一次
    QString m_lastLinkError;

    // 当前文件
    QString m_path;
    QByteArray m_pcm;
    Segments m_segments;
    int m_nextSegment = 0;
    bool m_busy = false;        // 整个批次进行中
    bool m_fileActive = false;  // 当前文件处理中
    bool m_cancelled = false;
    bool m_waitingSegment = false;
};
