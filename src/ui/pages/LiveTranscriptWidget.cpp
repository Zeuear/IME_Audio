#include "LiveTranscriptWidget.h"
#include "ui/delegates/TranscriptLineDelegate.h"
#include "ui/models/TranscriptLineModel.h"
#include "ui/models/TranscriptStore.h"
#include <QAction>
#include <QCheckBox>
#include "ui/controls/AnimatedCheckbox.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int kSaveDelayMs = 2000;   // 说话时每句都在追加，合并成一次写入
}

LiveTranscriptWidget::LiveTranscriptWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

LiveTranscriptWidget::~LiveTranscriptWidget()
{
    flushSave();
}

void LiveTranscriptWidget::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(10);
    m_toggleBtn = new QPushButton(this);
    m_toggleBtn->setCursor(Qt::PointingHandCursor);
    m_injectCheck = new AnimatedCheckBox(this);
    m_injectCheck->setChecked(true);
    m_timestampCheck = new AnimatedCheckBox(this);
    m_copyBtn = new QPushButton(this);
    m_exportBtn = new QPushButton(this);
    m_clearBtn = new QPushButton(this);
    bar->addWidget(m_toggleBtn);
    bar->addSpacing(8);
    m_injectCheck->hide();
    bar->addWidget(m_timestampCheck);
    bar->addStretch();
    bar->addWidget(m_copyBtn);
    bar->addWidget(m_exportBtn);
    bar->addWidget(m_clearBtn);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(kSaveDelayMs);
    connect(m_saveTimer, &QTimer::timeout, this, &LiveTranscriptWidget::flushSave);

    m_model = new TranscriptLineModel(this);
    m_list = new QListView(this);
    m_list->setObjectName(QStringLiteral("live_transcript_list"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    // 行高随宽度换行变化，窗口缩放时要重新排版
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMouseTracking(true);
    m_delegate = new TranscriptLineDelegate(m_list);
    m_list->setItemDelegate(m_delegate);
    m_list->setModel(m_model);
    m_list->viewport()->installEventFilter(this);

    auto* copyAction = new QAction(m_list);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setShortcutContext(Qt::WidgetShortcut);
    m_list->addAction(copyAction);
    connect(copyAction, &QAction::triggered, this, &LiveTranscriptWidget::copySelected);

    m_placeholder = new QLabel(m_list->viewport());
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setWordWrap(true);
    m_placeholder->setForegroundRole(QPalette::PlaceholderText);
    auto* placeholderLayout = new QVBoxLayout(m_list->viewport());
    placeholderLayout->addWidget(m_placeholder);

    layout->addLayout(bar);
    layout->addWidget(m_list, 1);

    connect(m_toggleBtn, &QPushButton::clicked, this, [this]() {
        if (m_recording) emit stopRequested();
        else emit startRequested();
    });
    connect(m_injectCheck, &QCheckBox::toggled, this, &LiveTranscriptWidget::injectToggled);
    connect(m_timestampCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_delegate->setShowTime(on);
        m_list->doItemsLayout();
        m_list->viewport()->update();
    });
    connect(m_copyBtn, &QPushButton::clicked, this, &LiveTranscriptWidget::copyAll);
    connect(m_exportBtn, &QPushButton::clicked, this, &LiveTranscriptWidget::exportText);
    connect(m_clearBtn, &QPushButton::clicked, this, &LiveTranscriptWidget::clearAll);
    connect(m_list, &QListView::doubleClicked, this, [this](const QModelIndex& idx) { copyRow(idx.row()); });
    for (auto signal : { &QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved }) {
        connect(m_model, signal, this, &LiveTranscriptWidget::updateButtons);
        connect(m_model, signal, this, &LiveTranscriptWidget::scheduleSave);
    }
    connect(m_model, &QAbstractItemModel::modelReset, this, &LiveTranscriptWidget::updateButtons);
    connect(m_model, &QAbstractItemModel::modelReset, this, &LiveTranscriptWidget::scheduleSave);

    retranslateUi();
    updateButtons();
}

void LiveTranscriptWidget::appendUtterance(const QString& text, const QDateTime& time)
{
    // 滚动条已在底部才跟随新内容；用户往上翻看前文时不抢走位置
    QScrollBar* bar = m_list->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 4;
    m_model->add(text, time.time());
    if (atBottom) m_list->scrollToBottom();
}

void LiveTranscriptWidget::setRecording(bool recording)
{
    if (m_recording == recording) return;
    m_recording = recording;
    updateButtons();
}

bool LiveTranscriptWidget::injectToForeground() const
{
    return m_injectCheck->isChecked();
}

void LiveTranscriptWidget::setInjectToForeground(bool on)
{
    m_injectCheck->setChecked(on);
}

bool LiveTranscriptWidget::withTime() const
{
    return m_timestampCheck->isChecked();
}

void LiveTranscriptWidget::updateButtons()
{
    m_toggleBtn->setText(m_recording ? tr("Stop") : tr("Start"));
    const bool has = m_model->rowCount() > 0;
    m_copyBtn->setEnabled(has);
    m_exportBtn->setEnabled(has);
    m_clearBtn->setEnabled(has);
    m_placeholder->setVisible(!has);
}

void LiveTranscriptWidget::copyAll()
{
    const QString text = m_model->allText(withTime());
    if (!text.isEmpty()) QGuiApplication::clipboard()->setText(text);
}

void LiveTranscriptWidget::copySelected()
{
    QList<int> rows;
    for (const QModelIndex& idx : m_list->selectionModel()->selectedIndexes()) rows << idx.row();
    std::sort(rows.begin(), rows.end());
    const QString text = m_model->linesText(rows, withTime());
    if (!text.isEmpty()) QGuiApplication::clipboard()->setText(text);
}

// 单行复制只取句子本身，方便直接粘贴到别处
void LiveTranscriptWidget::copyRow(int row)
{
    const QString text = m_model->textAt(row);
    if (!text.isEmpty()) QGuiApplication::clipboard()->setText(text);
}

void LiveTranscriptWidget::exportText()
{
    const QString text = m_model->allText(withTime());
    if (text.isEmpty()) return;
    const QString suggested = QStringLiteral("transcript_%1.txt")
                                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Transcript"), suggested,
                                                      tr("Text files (*.txt)"));
    if (path.isEmpty()) return;
    if (!TranscriptStore::exportTxt(path, text)) {
        emit errorOccurred(QStringLiteral("导出失败"), QStringLiteral("无法写入所选位置，请换一个文件夹再试"));
    }
}

void LiveTranscriptWidget::setSessionFile(const QString& path)
{
    m_store = std::make_unique<TranscriptStore>(path);

    const QString last = m_store->load();
    if (!last.isEmpty()) {
        m_restoring = true;
        m_model->restore(last);
        m_restoring = false;
        m_list->scrollToBottom();
    }
    // 窗口销毁不一定发生在退出之前，退出时无论如何补写一次
    connect(qApp, &QCoreApplication::aboutToQuit, this, &LiveTranscriptWidget::flushSave, Qt::UniqueConnection);
}

void LiveTranscriptWidget::scheduleSave()
{
    if (!m_store || m_restoring) return;
    m_dirty = true;
    m_saveTimer->start();
}

void LiveTranscriptWidget::flushSave()
{
    if (!m_store || !m_dirty) return;
    m_dirty = false;
    m_saveTimer->stop();
    const QString text = m_model->serialize();
    if (text.isEmpty()) {
        m_store->remove();
        return;
    }
    if (!m_store->save(text)) {
        emit errorOccurred(QStringLiteral("稿子保存失败"),
                           QStringLiteral("无法写入程序数据目录，请检查磁盘空间和目录权限；内容仍在窗口里，可先复制或导出"));
    }
}

void LiveTranscriptWidget::clearAll()
{
    m_model->clear();
    // 清空是用户明确要抹掉稿子：磁盘上的备份也一并删除，不留隐私内容
    if (m_store) m_store->remove();
    m_dirty = false;
    m_saveTimer->stop();
}

bool LiveTranscriptWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_list->viewport() && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        const QPoint pos = me->position().toPoint();
        const QModelIndex idx = m_list->indexAt(pos);
        if (idx.isValid() && TranscriptLineDelegate::copyRect(m_list->visualRect(idx)).contains(pos)) {
            copyRow(idx.row());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void LiveTranscriptWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QWidget::changeEvent(event);
}

void LiveTranscriptWidget::retranslateUi()
{
    m_injectCheck->setLabelText(tr("Also type into the foreground window"));
    m_injectCheck->setToolTip(tr("Turn off when you are typing yourself, so the transcript only goes here"));
    m_timestampCheck->setLabelText(tr("Timestamps"));
    m_timestampCheck->setToolTip(tr("Show the time of each line and include it when copying all or exporting"));
    m_copyBtn->setText(tr("Copy"));
    m_copyBtn->setToolTip(tr("Copy all lines to the clipboard"));
    m_exportBtn->setText(tr("Export"));
    m_clearBtn->setText(tr("Clear"));
    m_delegate->setCopyText(tr("Copy"));
    m_placeholder->setText(tr("Each sentence appears here on its own line as you speak. Hover a line and click Copy, or double-click it, to copy that line."));
    updateButtons();
}
