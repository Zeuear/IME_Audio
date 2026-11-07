#include "LiveTranscriptWidget.h"
#include "ui/models/TranscriptDocument.h"
#include "ui/models/TranscriptStore.h"
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

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
    m_injectCheck = new QCheckBox(this);
    m_injectCheck->setChecked(true);   // 默认同时输入到前台，不需要时再关
    m_timestampCheck = new QCheckBox(this);
    m_copyBtn = new QPushButton(this);
    m_exportBtn = new QPushButton(this);
    m_clearBtn = new QPushButton(this);
    bar->addWidget(m_toggleBtn);
    bar->addSpacing(8);
    bar->addWidget(m_injectCheck);
    bar->addWidget(m_timestampCheck);
    bar->addStretch();
    bar->addWidget(m_copyBtn);
    bar->addWidget(m_exportBtn);
    bar->addWidget(m_clearBtn);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(kSaveDelayMs);
    connect(m_saveTimer, &QTimer::timeout, this, &LiveTranscriptWidget::flushSave);

    m_edit = new QPlainTextEdit(this);
    m_edit->setObjectName(QStringLiteral("live_transcript_edit"));
    m_edit->setFrameShape(QFrame::NoFrame);
    m_transcript = std::make_unique<TranscriptDocument>(m_edit->document());

    layout->addLayout(bar);
    layout->addWidget(m_edit, 1);

    connect(m_toggleBtn, &QPushButton::clicked, this, [this]() {
        if (m_recording) emit stopRequested();
        else emit startRequested();
    });
    connect(m_injectCheck, &QCheckBox::toggled, this, &LiveTranscriptWidget::injectToggled);
    connect(m_timestampCheck, &QCheckBox::toggled, this,
            [this](bool on) { m_transcript->setTimestampsVisible(on); });
    connect(m_copyBtn, &QPushButton::clicked, this, &LiveTranscriptWidget::copyAll);
    connect(m_exportBtn, &QPushButton::clicked, this, &LiveTranscriptWidget::exportText);
    connect(m_clearBtn, &QPushButton::clicked, this, &LiveTranscriptWidget::clearAll);
    connect(m_edit, &QPlainTextEdit::textChanged, this, &LiveTranscriptWidget::updateButtons);
    connect(m_edit, &QPlainTextEdit::textChanged, this, &LiveTranscriptWidget::scheduleSave);

    retranslateUi();
    updateButtons();
}

void LiveTranscriptWidget::appendUtterance(const QString& text, const QDateTime& time)
{
    // 滚动条已在底部才跟随新内容；用户往上翻看前文时不抢走位置
    QScrollBar* bar = m_edit->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 4;
    m_transcript->appendUtterance(text, time);
    if (atBottom) bar->setValue(bar->maximum());
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

void LiveTranscriptWidget::updateButtons()
{
    m_toggleBtn->setText(m_recording ? tr("Stop") : tr("Start"));
    const bool has = !m_edit->document()->isEmpty();
    m_copyBtn->setEnabled(has);
    m_exportBtn->setEnabled(has);
    m_clearBtn->setEnabled(has);
}

void LiveTranscriptWidget::copyAll()
{
    const QString text = m_edit->toPlainText();
    if (!text.isEmpty()) QGuiApplication::clipboard()->setText(text);
}

void LiveTranscriptWidget::exportText()
{
    const QString text = m_edit->toPlainText();
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

    // 恢复上次的稿子：崩溃或误关之后不丢
    const QString last = m_store->load();
    if (!last.isEmpty()) {
        m_restoring = true;
        m_edit->setPlainText(last);
        m_restoring = false;
        m_edit->moveCursor(QTextCursor::End);
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
    const QString text = m_edit->toPlainText();
    if (text.isEmpty()) {
        m_store->remove();   // 空稿子不留文件
        return;
    }
    if (!m_store->save(text)) {
        emit errorOccurred(QStringLiteral("稿子保存失败"),
                           QStringLiteral("无法写入程序数据目录，请检查磁盘空间和目录权限；内容仍在窗口里，可先复制或导出"));
    }
}

void LiveTranscriptWidget::clearAll()
{
    m_edit->clear();
    // 清空是用户明确要抹掉稿子：磁盘上的备份也一并删除，不留隐私内容
    if (m_store) m_store->remove();
    m_dirty = false;
    m_saveTimer->stop();
}

void LiveTranscriptWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QWidget::changeEvent(event);
}

void LiveTranscriptWidget::retranslateUi()
{
    m_injectCheck->setText(tr("Also type into the foreground window"));
    m_injectCheck->setToolTip(tr("Turn off when you are typing yourself, so the transcript only goes here"));
    m_timestampCheck->setText(tr("Timestamps"));
    m_copyBtn->setText(tr("Copy"));
    m_exportBtn->setText(tr("Export"));
    m_clearBtn->setText(tr("Clear"));
    m_edit->setPlaceholderText(tr("The transcript appears here as you speak. You can edit it at any time; new text is always added at the end."));
    updateButtons();
}
