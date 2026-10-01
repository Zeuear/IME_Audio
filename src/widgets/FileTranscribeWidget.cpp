#include "FileTranscribeWidget.h"
#include "FileTaskDelegate.h"
#include <QAudioDecoder>
#include <QAudioBuffer>
#include <QAudioFormat>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QMenu>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QClipboard>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>
#include "cxx-api.h"
#include "AppConfig.h"
#include "TranscriptionService.h"
#include "sherpa/SherpaManager.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"

namespace {
constexpr int kSampleRate = 16000;
constexpr float kMaxSegmentSec = 30.0f;   // 单段上限，云端后端也能接受
}

FileTranscribeWidget::FileTranscribeWidget(QWidget* parent) : QWidget(parent)
{
    setAcceptDrops(true);
    setupUi();
}

void FileTranscribeWidget::setServices(TranscriptionService* transcription, SherpaManager* sherpa, const AppConfig* config)
{
    m_transcription = transcription;
    m_sherpa = sherpa;
    m_config = config;
    connect(m_transcription, &TranscriptionService::fileSegmentFinished,
            this, &FileTranscribeWidget::onSegmentFinished);
}

void FileTranscribeWidget::setupUi()
{
    m_model = new FileTaskModel(this);

    m_addBtn = new QPushButton(this);
    m_transcribeBtn = new QPushButton(this);
    m_cancelBtn = new QPushButton(this);
    m_clearBtn = new QPushButton(this);
    m_exportBtn = new QPushButton(this);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_transcribeBtn);
    toolbar->addWidget(m_cancelBtn);
    toolbar->addWidget(m_clearBtn);
    toolbar->addStretch();
    toolbar->addWidget(m_exportBtn);

    // 上半部分：列表为空时显示拖入提示，有文件后切到卡片列表
    m_hintLabel = new QLabel(this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QStringLiteral("QLabel { border: 2px dashed palette(mid); border-radius: 10px; }"));

    m_list = new QListView(this);
    m_list->setModel(m_model);
    m_list->setItemDelegate(new FileTaskDelegate(m_list));
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setUniformItemSizes(true);
    // 方块平铺：一行排满后自动换到下一行
    // 必须用 IconMode：ListMode 会把条目拉伸成整行宽度，不会平铺
    m_list->setViewMode(QListView::IconMode);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setSpacing(0);
    m_list->setMovement(QListView::Static);
    // IconMode 会让列表自己接收拖放并吞掉文件，关掉后拖放才会交给本控件的 dropEvent
    m_list->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_list->setAcceptDrops(false);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setMouseTracking(true);   // 悬停显示失败原因

    m_listStack = new QStackedWidget(this);
    m_listStack->addWidget(m_hintLabel);
    m_listStack->addWidget(m_list);

    // 下半部分：所选文件的文本预览
    m_previewTitle = new QLabel(this);
    m_copyBtn = new QPushButton(this);
    m_preview = new QPlainTextEdit(this);
    m_preview->setReadOnly(true);

    auto* previewHeader = new QHBoxLayout;
    previewHeader->addWidget(m_previewTitle, 1);
    previewHeader->addWidget(m_copyBtn);

    auto* previewPanel = new QWidget(this);
    auto* previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->addLayout(previewHeader);
    previewLayout->addWidget(m_preview, 1);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_listStack);
    splitter->addWidget(previewPanel);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    m_statusLabel = new QLabel(this);
    m_openFolderBtn = new QPushButton(this);
    m_openFolderBtn->hide();

    auto* statusRow = new QHBoxLayout;
    statusRow->addWidget(m_statusLabel, 1);
    statusRow->addWidget(m_openFolderBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(splitter, 1);
    layout->addLayout(statusRow);

    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(80);
    connect(m_animTimer, &QTimer::timeout, this, [this]() {
        if (m_currentRow >= 0 && m_currentRow < m_model->rowCount()
            && m_model->task(m_currentRow).state == FileTask::State::Decoding)
            m_list->viewport()->update();
    });

    connect(m_addBtn, &QPushButton::clicked, this, &FileTranscribeWidget::chooseFiles);
    connect(m_transcribeBtn, &QPushButton::clicked, this, &FileTranscribeWidget::transcribeClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &FileTranscribeWidget::cancel);
    connect(m_clearBtn, &QPushButton::clicked, this, &FileTranscribeWidget::clearAll);
    connect(m_exportBtn, &QPushButton::clicked, this, &FileTranscribeWidget::exportResults);
    connect(m_copyBtn, &QPushButton::clicked, this, &FileTranscribeWidget::copyPreview);
    connect(m_openFolderBtn, &QPushButton::clicked, this, [this]() {
        if (!m_lastExportDir.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastExportDir));
    });
    connect(m_list, &QListView::customContextMenuRequested, this, &FileTranscribeWidget::showContextMenu);
    connect(m_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { updateButtons(); });
    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]() { updatePreview(); });

    connect(m_model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& top, const QModelIndex& bottom) {
        const int cur = m_list->currentIndex().row();
        if (cur >= top.row() && cur <= bottom.row()) updatePreview();
        updateButtons();
    });
    auto onStructureChanged = [this]() {
        m_listStack->setCurrentIndex(m_model->rowCount() > 0 ? 1 : 0);
        updatePreview();
        updateButtons();
    };
    connect(m_model, &QAbstractItemModel::rowsInserted, this, onStructureChanged);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, onStructureChanged);
    connect(m_model, &QAbstractItemModel::modelReset, this, onStructureChanged);

    retranslateUi();
    updateButtons();
}

void FileTranscribeWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        this->retranslateUi();
    }
    QWidget::changeEvent(event);
}

void FileTranscribeWidget::retranslateUi()
{
    m_addBtn->setText(tr("Add Files"));
    m_transcribeBtn->setText(tr("Transcribe"));
    m_cancelBtn->setText(tr("Cancel"));
    m_clearBtn->setText(tr("Clear"));
    m_exportBtn->setText(tr("Export"));
    m_hintLabel->setText(tr("Drop audio files here, or click \"Add Files\""));
    m_copyBtn->setText(tr("Copy"));
    m_preview->setPlaceholderText(tr("Select a file to view its transcription"));
    m_openFolderBtn->setText(tr("Open Folder"));

    // 预览标题未选中时是"Transcription"；卡片状态文字由模型在绘制时 tr，重绘即可刷新
    updatePreview();
    m_list->viewport()->update();
}

// ---------------------------------------------------------------- 列表与按钮

void FileTranscribeWidget::dragEnterEvent(QDragEnterEvent* event)
{
    LOG_INFO(QString("[DIAG-DND] dragEnter formats=%1 hasUrls=%2 action=%3")
                 .arg(event->mimeData()->formats().join(','))
                 .arg(event->mimeData()->hasUrls()).arg(int(event->proposedAction())));
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void FileTranscribeWidget::dropEvent(QDropEvent* event)
{
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        LOG_INFO(QString("[DIAG-DND] drop url=%1 local=%2").arg(url.toString()).arg(url.isLocalFile()));
        if (url.isLocalFile()) paths << url.toLocalFile();
    }
    addFiles(paths);
}

void FileTranscribeWidget::chooseFiles()
{
    addFiles(QFileDialog::getOpenFileNames(
        this, tr("Choose Audio Files"), QString(),
        tr("Audio Files (*.wav *.mp3 *.m4a *.aac *.flac *.ogg *.opus *.wma *.mp4 *.mkv);;All Files (*)")));
}

void FileTranscribeWidget::addFiles(const QStringList& paths)
{
    if (paths.isEmpty()) return;
    const int added = m_model->addPaths(paths);
    if (added == 0) {
        m_statusLabel->setText(tr("No new files added"));
        return;
    }
    // 转录中追加的文件会在当前队列清空后并入本批
    if (m_busy) m_fileTotal += added;
    m_statusLabel->setText(tr("Added %1 files").arg(added));
}

QList<int> FileTranscribeWidget::selectedRows() const
{
    QList<int> rows;
    for (const QModelIndex& idx : m_list->selectionModel()->selectedRows())
        rows << idx.row();
    std::sort(rows.begin(), rows.end());
    return rows;
}

void FileTranscribeWidget::removeSelected()
{
    if (m_busy) return;
    m_model->removeRows(selectedRows());
}

void FileTranscribeWidget::clearAll()
{
    if (m_busy) return;
    m_model->clear();
    m_statusLabel->clear();
}

void FileTranscribeWidget::showContextMenu(const QPoint& pos)
{
    const QModelIndex idx = m_list->indexAt(pos);
    if (!idx.isValid()) return;
    if (!m_list->selectionModel()->isSelected(idx))
        m_list->setCurrentIndex(idx);   // 右键未选中的卡片时先选中它

    QMenu menu(this);
    QAction* retry = menu.addAction(tr("Re-transcribe"));
    QAction* remove = menu.addAction(tr("Remove"));
    retry->setEnabled(!m_busy);
    remove->setEnabled(!m_busy);

    const QAction* chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (chosen == retry) startRun(selectedRows());
    else if (chosen == remove) removeSelected();
}

void FileTranscribeWidget::updatePreview()
{
    const QModelIndex idx = m_list->currentIndex();
    if (!idx.isValid()) {
        m_previewTitle->setText(tr("Transcription"));
        m_preview->clear();
        m_copyBtn->setEnabled(false);
        return;
    }

    const FileTask& t = m_model->task(idx.row());
    m_previewTitle->setText(QFileInfo(t.path).fileName());
    const QString text = t.state == FileTask::State::Failed ? t.error : t.text;
    if (m_preview->toPlainText() != text) m_preview->setPlainText(text);
    m_copyBtn->setEnabled(!t.text.isEmpty());
}

void FileTranscribeWidget::updateButtons()
{
    bool hasRunnable = false;
    const QList<int> selected = selectedRows();
    if (selected.isEmpty()) {
        hasRunnable = m_model->count(FileTask::State::Pending) > 0 || m_model->count(FileTask::State::Failed) > 0;
    } else {
        for (int row : selected) {
            if (m_model->task(row).state != FileTask::State::Done) { hasRunnable = true; break; }
        }
    }

    m_addBtn->setEnabled(true);   // 转录中也允许追加
    m_transcribeBtn->setEnabled(!m_busy && hasRunnable);
    m_cancelBtn->setEnabled(m_busy && !m_cancelled);
    m_clearBtn->setEnabled(!m_busy && m_model->rowCount() > 0);
    m_exportBtn->setEnabled(m_model->count(FileTask::State::Done) > 0);
}

void FileTranscribeWidget::setBusy(bool busy)
{
    m_busy = busy;
    if (busy) m_animTimer->start();
    else m_animTimer->stop();
    updateButtons();
}

// ---------------------------------------------------------------- 转录批次

void FileTranscribeWidget::transcribeClicked()
{
    QList<int> rows;
    const QList<int> selected = selectedRows();
    if (!selected.isEmpty()) {
        // 有选中：只处理选中的，已成功的跳过（要重做请用右键"重新转录"）
        for (int row : selected) {
            if (m_model->task(row).state != FileTask::State::Done) rows << row;
        }
    } else {
        for (int row = 0; row < m_model->rowCount(); ++row) {
            const auto state = m_model->task(row).state;
            if (state == FileTask::State::Pending || state == FileTask::State::Failed) rows << row;
        }
    }
    startRun(rows);
}

void FileTranscribeWidget::startRun(const QList<int>& rows)
{
    if (m_busy || !m_transcription || rows.isEmpty()) return;

    for (int row : rows) m_model->reset(row);

    m_runRows = rows;
    m_runWatermark = m_model->rowCount();
    m_fileTotal = rows.size();
    m_fileDone = 0;
    m_okCount = m_failedCount = m_noSpeechCount = 0;
    m_lastError.clear();
    m_cancelled = false;
    m_openFolderBtn->hide();
    setBusy(true);
    m_transcription->setFileMode(true);
    startNextFile();
}

void FileTranscribeWidget::collectAppendedRows()
{
    for (int row = m_runWatermark; row < m_model->rowCount(); ++row) {
        if (m_model->task(row).state == FileTask::State::Pending) m_runRows << row;
    }
    m_runWatermark = m_model->rowCount();
}

void FileTranscribeWidget::startNextFile()
{
    if (m_cancelled) { finishAll(); return; }
    if (m_runRows.isEmpty()) collectAppendedRows();
    if (m_runRows.isEmpty()) { finishAll(); return; }

    m_currentRow = m_runRows.takeFirst();
    m_path = m_model->task(m_currentRow).path;
    m_pcm.clear();
    m_segments.clear();
    m_nextSegment = 0;
    m_waitingSegment = false;
    m_fileActive = true;
    m_model->setState(m_currentRow, FileTask::State::Decoding);
    m_statusLabel->setText(tr("Processing file %1/%2").arg(m_fileDone + 1).arg(m_fileTotal));
    m_list->scrollTo(m_model->index(m_currentRow));

    // 每个文件新建解码器：复用同一个 QAudioDecoder 时，第二个文件起会立刻 finished 且没有任何数据
    if (m_decoder) {
        m_decoder->disconnect(this);
        m_decoder->stop();
        m_decoder->deleteLater();
    }
    m_decoder = new QAudioDecoder(this);
    connect(m_decoder, &QAudioDecoder::bufferReady, this, &FileTranscribeWidget::onDecodeBuffer);
    connect(m_decoder, &QAudioDecoder::finished, this, &FileTranscribeWidget::onDecodeFinished);
    connect(m_decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this,
            [this](QAudioDecoder::Error) {
                if (!m_fileActive) return;
                fail(m_decoder->errorString());
            });

    QAudioFormat fmt;
    fmt.setSampleRate(kSampleRate);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);
    m_decoder->setAudioFormat(fmt);
    m_decoder->setSource(QUrl::fromLocalFile(m_path));
    m_decoder->start();
}

void FileTranscribeWidget::onDecodeBuffer()
{
    while (m_decoder->bufferAvailable()) {
        const QAudioBuffer buf = m_decoder->read();
        if (!buf.isValid()) break;
        m_pcm.append(buf.constData<char>(), buf.byteCount());
    }
}

void FileTranscribeWidget::onDecodeFinished()
{
    if (!m_fileActive) return;
    onDecodeBuffer();
    if (m_cancelled) { finishFile(FileTask::State::Pending); return; }
    if (m_pcm.isEmpty()) {
        LOG_WARN(QString("Decoder finished with no data: error=%1 '%2' duration=%3ms source=%4")
                     .arg(int(m_decoder->error())).arg(m_decoder->errorString())
                     .arg(m_decoder->duration()).arg(m_decoder->source().toString()));
        fail(tr("No audio data decoded"));
        return;
    }

    if (!m_vadWatcher) {
        m_vadWatcher = new QFutureWatcher<Segments>(this);
        connect(m_vadWatcher, &QFutureWatcher<Segments>::finished,
                this, &FileTranscribeWidget::onSegmentsReady);
    }
    // 拷贝一份配置交给后台线程，避免与界面线程竞争
    const AppConfig cfg(*m_config);
    const QByteArray pcm = m_pcm;
    QString* err = &m_vadError;
    m_vadError.clear();
    m_vadWatcher->setFuture(QtConcurrent::run([pcm, cfg, err]() {
        return segmentByVad(pcm, cfg, err);
    }));
}

FileTranscribeWidget::Segments FileTranscribeWidget::segmentByVad(const QByteArray& pcm,
                                                                  const AppConfig& config,
                                                                  QString* error)
{
    Segments out;
    QString vadPath = config.sherpa.vadPath;
    if (!QFile::exists(vadPath)) vadPath = AppPaths::vadModelFile();
    if (!QFile::exists(vadPath)) {
        *error = QStringLiteral("VAD 模型缺失，请先下载模型");
        return out;
    }

    try {
        sherpa_onnx::cxx::VadModelConfig vc;
        vc.silero_vad.model = vadPath.toStdString();
        vc.silero_vad.threshold = static_cast<float>(config.audio.voiceThreshold) / 1000;
        vc.silero_vad.min_silence_duration = 0.5f;
        vc.silero_vad.min_speech_duration = 0.25f;
        vc.silero_vad.max_speech_duration = kMaxSegmentSec;
        vc.sample_rate = kSampleRate;

        auto vad = sherpa_onnx::cxx::VoiceActivityDetector::Create(vc, 60.0f);

        auto drain = [&]() {
            while (!vad.IsEmpty()) {
                auto seg = vad.Front();
                QByteArray bytes(static_cast<int>(seg.samples.size() * sizeof(int16_t)), Qt::Uninitialized);
                auto* dst = reinterpret_cast<int16_t*>(bytes.data());
                for (size_t i = 0; i < seg.samples.size(); ++i)
                    dst[i] = static_cast<int16_t>(qBound(-1.0f, seg.samples[i], 1.0f) * 32767.0f);
                out.append(bytes);
                vad.Pop();
            }
        };

        const auto* s16 = reinterpret_cast<const int16_t*>(pcm.constData());
        const int total = pcm.size() / 2;
        constexpr int kChunk = 512 * 8;
        std::vector<float> buf(kChunk);
        for (int pos = 0; pos < total; pos += kChunk) {
            const int n = qMin(kChunk, total - pos);
            for (int i = 0; i < n; ++i) buf[i] = s16[pos + i] / 32768.0f;
            vad.AcceptWaveform(buf.data(), n);
            drain();
        }
        vad.Flush();
        drain();
    }
    catch (const std::exception& e) {
        *error = QString::fromLocal8Bit(e.what());
    }
    return out;
}

void FileTranscribeWidget::onSegmentsReady()
{
    if (!m_fileActive) return;
    if (m_cancelled) { finishFile(FileTask::State::Pending); return; }
    if (!m_vadError.isEmpty()) { fail(m_vadError); return; }

    m_segments = m_vadWatcher->result();
    m_pcm.clear();
    if (m_segments.isEmpty()) { finishFile(FileTask::State::NoSpeech); return; }

    m_nextSegment = 0;
    m_model->setProgress(m_currentRow, 0, m_segments.size());

    if (m_config->backend == AsrBackendKind::Sherpa) {
        m_sherpa->pauseIdleTimer();
        // 模型可能已被空闲卸载；加载与转录任务在 worker 队列里串行，先入队加载即可
        if (!m_sherpa->isModelLoaded()) m_sherpa->loadModelAsync(*m_config, false);
    }
    transcribeNext();
}

void FileTranscribeWidget::transcribeNext()
{
    if (m_cancelled) { finishFile(FileTask::State::Pending); return; }
    if (m_nextSegment >= m_segments.size()) {
        finishFile(m_model->task(m_currentRow).text.isEmpty() ? FileTask::State::NoSpeech
                                                              : FileTask::State::Done);
        return;
    }

    m_waitingSegment = true;
    m_transcription->transcribe(m_segments.at(m_nextSegment), kSampleRate, 1, 16);
}

void FileTranscribeWidget::onSegmentFinished(bool ok, const QString&, const QString& finalText, const QString& err)
{
    if (!m_fileActive || !m_waitingSegment) return;
    m_waitingSegment = false;

    if (!ok) {
        fail(err);
        return;
    }
    const QString text = finalText.trimmed();
    if (!text.isEmpty()) m_model->appendText(m_currentRow, text);
    m_model->setProgress(m_currentRow, ++m_nextSegment, m_segments.size());
    transcribeNext();
}

void FileTranscribeWidget::cancel()
{
    m_cancelled = true;
    m_runRows.clear();
    m_statusLabel->setText(tr("Cancelling..."));
    updateButtons();
    if (m_decoder && m_decoder->isDecoding()) {
        m_decoder->stop();
        finishFile(FileTask::State::Pending);
    }
    // 其余阶段在当前步骤完成后由 transcribeNext / onSegmentsReady 检查标志退出
}

void FileTranscribeWidget::finishFile(FileTask::State state, const QString& error)
{
    if (!m_fileActive) return;
    m_fileActive = false;
    if (m_decoder && m_decoder->isDecoding()) m_decoder->stop();

    m_waitingSegment = false;
    m_pcm.clear();
    m_segments.clear();
    ++m_fileDone;

    switch (state) {
    case FileTask::State::Done:
        ++m_okCount;
        m_model->setState(m_currentRow, FileTask::State::Done);
        break;
    case FileTask::State::Failed:
        ++m_failedCount;
        m_lastError = error;
        m_model->setFailed(m_currentRow, error);
        break;
    case FileTask::State::NoSpeech:
        ++m_noSpeechCount;
        m_model->setState(m_currentRow, FileTask::State::NoSpeech);
        break;
    default:   // 取消：退回待处理
        m_model->reset(m_currentRow);
        break;
    }
    m_currentRow = -1;

    // 延后一拍再开始下一个：避免在解码器/Future 的信号回调里重入解码器
    QTimer::singleShot(0, this, &FileTranscribeWidget::startNextFile);
}

void FileTranscribeWidget::fail(const QString& cause)
{
    LOG_ERROR(QString("File transcription failed (%1): %2").arg(m_path, cause));
    finishFile(FileTask::State::Failed, cause);
}

void FileTranscribeWidget::finishAll()
{
    m_transcription->setFileMode(false);
    if (m_config->backend == AsrBackendKind::Sherpa) m_sherpa->resumeIdleTimer();

    if (m_cancelled)
        m_statusLabel->setText(tr("Cancelled"));
    else
        m_statusLabel->setText(tr("Finished: %1 succeeded, %2 failed, %3 no speech")
                                   .arg(m_okCount).arg(m_failedCount).arg(m_noSpeechCount));
    const bool hasFailure = m_failedCount > 0 && !m_cancelled;
    const int failed = m_failedCount;
    const QString lastError = m_lastError;
    const bool batch = m_fileTotal > 1;
    setBusy(false);

    // 批量时错误只汇总弹一次，避免同一原因（如 Key 未配置）连弹 N 次
    if (hasFailure) {
        emit errorOccurred(QStringLiteral("文件转录失败"),
                           batch ? QStringLiteral("%1 个文件转录失败，最后一个错误：%2").arg(failed).arg(lastError)
                                 : lastError);
    }
}

// ---------------------------------------------------------------- 导出

void FileTranscribeWidget::copyPreview()
{
    const QModelIndex idx = m_list->currentIndex();
    if (!idx.isValid()) return;
    QGuiApplication::clipboard()->setText(m_model->task(idx.row()).text);
    m_statusLabel->setText(tr("Copied to clipboard"));
}

bool FileTranscribeWidget::writeTextFile(const QString& path, const QString& text)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    f.write(text.toUtf8());
    return true;
}

void FileTranscribeWidget::exportResults()
{
    // 有选中就只导出选中的成功项，否则导出全部成功项
    QList<int> rows;
    for (int row : selectedRows()) {
        if (m_model->task(row).state == FileTask::State::Done) rows << row;
    }
    if (rows.isEmpty()) {
        for (int row = 0; row < m_model->rowCount(); ++row) {
            if (m_model->task(row).state == FileTask::State::Done) rows << row;
        }
    }
    if (rows.isEmpty()) return;

    const QString startDir = m_lastExportDir.isEmpty() ? QFileInfo(m_model->task(rows.first()).path).absolutePath()
                                                       : m_lastExportDir;
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose Export Folder"), startDir);
    if (dir.isEmpty()) return;

    // 按音频名称导出同名 txt；重名时加序号，不覆盖已有文件
    int written = 0;
    for (int row : std::as_const(rows)) {
        const FileTask& t = m_model->task(row);
        const QString base = QFileInfo(t.path).completeBaseName();
        QString target = QDir(dir).filePath(base + ".txt");
        for (int i = 2; QFile::exists(target); ++i)
            target = QDir(dir).filePath(QStringLiteral("%1 (%2).txt").arg(base).arg(i));
        if (writeTextFile(target, t.text)) ++written;
    }

    m_lastExportDir = dir;
    m_statusLabel->setText(tr("Exported %1 files to %2").arg(written).arg(dir));
    m_openFolderBtn->show();
}
