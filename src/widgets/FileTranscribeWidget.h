#pragma once
#include <QWidget>
#include <QByteArray>
#include <QList>
#include <QFutureWatcher>
#include "FileTaskModel.h"

class QAudioDecoder;
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
// 转录流程：解码为 16k 单声道 PCM → VAD 切段 → 逐段送入转录后端，按队列依次处理各文件。
// 转录期间 TranscriptionService 处于文件模式，结果不会走听写工作流（不会被注入前台窗口）。
class FileTranscribeWidget : public QWidget {
    Q_OBJECT
public:
    explicit FileTranscribeWidget(QWidget* parent = nullptr);

    // 依赖由 MainWin 注入（控件在 .ui 中提升，无法走带参构造）
    void setServices(TranscriptionService* transcription, SherpaManager* sherpa, const AppConfig* config);

    bool isBusy() const { return m_busy; }

signals:
    // 统一错误通知
    void errorOccurred(const QString& title, const QString& cause = {});

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    using Segments = QList<QByteArray>;

    void setupUi();

    // 列表与按钮
    void chooseFiles();
    void addFiles(const QStringList& paths);
    QList<int> selectedRows() const;
    void removeSelected();
    void clearAll();
    void showContextMenu(const QPoint& pos);
    void updatePreview();
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

    void onDecodeBuffer();
    void onDecodeFinished();
    void onSegmentsReady();
    void transcribeNext();
    void onSegmentFinished(bool ok, const QString& rawText, const QString& finalText, const QString& err);

    // 导出
    void exportResults();
    void copyPreview();
    static bool writeTextFile(const QString& path, const QString& text);

    static Segments segmentByVad(const QByteArray& pcm, const AppConfig& config, QString* error);

    TranscriptionService* m_transcription = nullptr;
    SherpaManager* m_sherpa = nullptr;
    const AppConfig* m_config = nullptr;

    QAudioDecoder* m_decoder = nullptr;
    QFutureWatcher<Segments>* m_vadWatcher = nullptr;
    QString m_vadError;

    FileTaskModel* m_model = nullptr;
    QStackedWidget* m_listStack = nullptr;
    QListView* m_list = nullptr;
    QLabel* m_previewTitle = nullptr;
    QPlainTextEdit* m_preview = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_transcribeBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_copyBtn = nullptr;
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
