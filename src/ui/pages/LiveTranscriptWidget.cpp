#include "LiveTranscriptWidget.h"
#include "ui/models/TranscriptDocument.h"
#include <QCheckBox>
#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

LiveTranscriptWidget::LiveTranscriptWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

LiveTranscriptWidget::~LiveTranscriptWidget() = default;

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
    m_clearBtn = new QPushButton(this);
    bar->addWidget(m_toggleBtn);
    bar->addSpacing(8);
    bar->addWidget(m_injectCheck);
    bar->addWidget(m_timestampCheck);
    bar->addStretch();
    bar->addWidget(m_copyBtn);
    bar->addWidget(m_clearBtn);

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
    connect(m_clearBtn, &QPushButton::clicked, m_edit, &QPlainTextEdit::clear);
    connect(m_edit, &QPlainTextEdit::textChanged, this, &LiveTranscriptWidget::updateButtons);

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
    m_clearBtn->setEnabled(has);
}

void LiveTranscriptWidget::copyAll()
{
    const QString text = m_edit->toPlainText();
    if (!text.isEmpty()) QGuiApplication::clipboard()->setText(text);
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
    m_clearBtn->setText(tr("Clear"));
    m_edit->setPlaceholderText(tr("The transcript appears here as you speak. You can edit it at any time; new text is always added at the end."));
    updateButtons();
}
