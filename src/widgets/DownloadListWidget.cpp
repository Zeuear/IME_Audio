#include "DownloadListWidget.h"

#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include "../sherpa/SherpaManager.h"
#include "../cuda/CudaInstaller.h"

ProjectGroupCard::ProjectGroupCard(const QString& repoId, const QString& displayName, int totalFiles, QWidget* parent)
    : QWidget(parent), m_repoId(repoId), m_totalFiles(totalFiles)
{
    setObjectName("projectGroupCard");
    setStyleSheet(
        "#projectGroupCard {"
        "  background-color: #17181c;"
        "  border: 1px solid #2a2b30;"
        "  border-radius: 10px;"
        "}"
    );

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 12, 16, 12);
    rootLayout->setSpacing(6);

    // 顶部标题行:项目名称 + 整体进度
    auto* headerLayout = new QHBoxLayout();
    m_titleLabel = new QLabel(displayName, this);
    m_titleLabel->setStyleSheet("color:#f0f0f2; font-size:13px; font-weight:600;");

    m_overallLabel = new QLabel(QString("0/%1").arg(totalFiles), this);
    m_overallLabel->setStyleSheet("color:#9a9ba3; font-size:12px;");
    m_overallLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addWidget(m_overallLabel);
    rootLayout->addLayout(headerLayout);

    m_overallBar = new QProgressBar(this);
    m_overallBar->setRange(0, 100);
    m_overallBar->setValue(0);
    m_overallBar->setFixedHeight(5);
    m_overallBar->setTextVisible(false);
    m_overallBar->setStyleSheet(
        "QProgressBar { background-color:#2a2b30; border-radius:2px; }"
        "QProgressBar::chunk { background-color:#d9b35d; border-radius:2px; }"
    );
    rootLayout->addWidget(m_overallBar);

    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("color:#2a2b30; background-color:#2a2b30;");
    divider->setFixedHeight(1);
    rootLayout->addWidget(divider);

    // ---- 文件列表区域 ----
    m_fileListLayout = new QVBoxLayout();
    m_fileListLayout->setSpacing(6);
    rootLayout->addLayout(m_fileListLayout);
}

ProjectGroupCard::FileRow& ProjectGroupCard::ensureFileRow(const QString& filename)
{
    auto it = m_fileRows.find(filename);
    if (it != m_fileRows.end()) return it.value();

    FileRow row;
    row.container = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(row.container);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    row.nameLabel = new QLabel(filename, row.container);
    row.nameLabel->setStyleSheet("color:#d0d0d3; font-size:12px;");
    row.nameLabel->setMinimumWidth(160);
    row.nameLabel->setToolTip(filename);

    row.progressBar = new QProgressBar(row.container);
    row.progressBar->setRange(0, 100);
    row.progressBar->setValue(0);
    row.progressBar->setFixedHeight(6);
    row.progressBar->setTextVisible(false);
    row.progressBar->setStyleSheet(
        "QProgressBar { background-color:#2a2b30; border-radius:3px; }"
        "QProgressBar::chunk { background-color:#5b8ee8; border-radius:3px; }"
    );

    row.statusLabel = new QLabel(tr("等待中"), row.container);
    row.statusLabel->setStyleSheet("color:#8a8b93; font-size:11px;");
    row.statusLabel->setFixedWidth(70);
    row.statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    rowLayout->addWidget(row.nameLabel);
    rowLayout->addWidget(row.progressBar, 1);
    rowLayout->addWidget(row.statusLabel);

    m_fileListLayout->addWidget(row.container);

    auto inserted = m_fileRows.insert(filename, row);
    return inserted.value();
}

void ProjectGroupCard::updateFileProgress(const QString& filename, qint64 received, qint64 total, int overallPercent)
{
    FileRow& row = ensureFileRow(filename);

    if (total > 0) {
        int pct = static_cast<int>(received * 100 / total);
        row.progressBar->setRange(0, 100);
        row.progressBar->setValue(pct);
        row.statusLabel->setText(QString("%1%").arg(pct));
    } else {
        row.progressBar->setRange(0, 0);
        row.statusLabel->setText(tr("下载中"));
    }

    m_overallBar->setValue(overallPercent);
}

void ProjectGroupCard::markFileFinished(const QString& filename)
{
    FileRow& row = ensureFileRow(filename);
    row.progressBar->setRange(0, 100);
    row.progressBar->setValue(100);
    row.statusLabel->setText(tr("完成"));
    row.statusLabel->setStyleSheet("color:#6fcf6f; font-size:11px;");

    m_finishedCount++;
    m_overallLabel->setText(QString("%1/%2").arg(m_finishedCount).arg(m_totalFiles));
}

void ProjectGroupCard::markFileError(const QString& filename, const QString& error)
{
    FileRow& row = ensureFileRow(filename);
    row.statusLabel->setText(tr("失败"));
    row.statusLabel->setStyleSheet("color:#e05c5c; font-size:11px;");
    row.statusLabel->setToolTip(error);
}

void ProjectGroupCard::updateGroupFinished(bool success, const QString& msg)
{
    if (success) {
        m_titleLabel->setText(m_titleLabel->text() + " " + tr("[已完成]"));
        m_overallBar->setValue(100);
    } else {
        m_titleLabel->setStyleSheet("color:#e05c5c; font-size:13px; font-weight:600;");
        m_overallLabel->setText(msg.isEmpty() ? tr("失败") : msg);
        m_overallLabel->setStyleSheet("color:#e05c5c; font-size:12px;");
    }
}

DownloadListWidget::DownloadListWidget(QWidget* parent): QWidget(parent)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget(scrollArea);
    m_cardListLayout = new QVBoxLayout(container);
    m_cardListLayout->setContentsMargins(12, 12, 12, 12);
    m_cardListLayout->setSpacing(10);
    m_cardListLayout->addStretch(1); 

    scrollArea->setWidget(container);
    rootLayout->addWidget(scrollArea);
}

void DownloadListWidget::setSherpaInstaller(SherpaInstaller* installer)
{
    m_installer = installer;

    connect(m_installer, &SherpaInstaller::installGroupStarted,   this, &DownloadListWidget::onInstallGroupStarted);
    connect(m_installer, &SherpaInstaller::installFileProgress,   this, &DownloadListWidget::onInstallFileProgress);
    connect(m_installer, &SherpaInstaller::installFileFinished,   this, &DownloadListWidget::onInstallFileFinished);
    connect(m_installer, &SherpaInstaller::installFileError,      this, &DownloadListWidget::onInstallFileError);
    connect(m_installer, &SherpaInstaller::installGroupFinished,  this, &DownloadListWidget::onInstallGroupFinished);
}

void DownloadListWidget::setCudaInstaller(CudaInstaller* installer)
{
    m_cudaInstaller = installer;

    connect(m_cudaInstaller, &CudaInstaller::installGroupStarted, this, &DownloadListWidget::onInstallGroupStarted);
    connect(m_cudaInstaller, &CudaInstaller::installFileProgress, this, &DownloadListWidget::onInstallFileProgress);
    connect(m_cudaInstaller, &CudaInstaller::installFileFinished, this, &DownloadListWidget::onInstallFileFinished);
    connect(m_cudaInstaller, &CudaInstaller::installFileError, this, &DownloadListWidget::onInstallFileError);
    connect(m_cudaInstaller, &CudaInstaller::installGroupFinished, this, &DownloadListWidget::onInstallGroupFinished);
}

ProjectGroupCard* DownloadListWidget::ensureCard(const QString& repoId, const QString& displayName, int totalFiles)
{
    auto it = m_cards.find(repoId);
    if (it != m_cards.end()) return it.value();

    auto* card = new ProjectGroupCard(repoId, displayName, totalFiles, this);
    m_cardListLayout->insertWidget(m_cardListLayout->count() - 1, card);
    m_cards.insert(repoId, card);
    return card;
}

void DownloadListWidget::onInstallGroupStarted(const QString& repoId, const QString& displayName, int totalFiles)
{
    ensureCard(repoId, displayName, totalFiles);
}


void DownloadListWidget::onInstallFileProgress(const QString& repoId, const QString& filename, qint64 recv, qint64 total, int overallPercent)
{
    if (auto* card = m_cards.value(repoId, nullptr)) {
        card->updateFileProgress(filename, recv, total, overallPercent);
    }
}

void DownloadListWidget::onInstallFileFinished(const QString& repoId, const QString& filename)
{
    if (auto* card = m_cards.value(repoId, nullptr)) {
        card->markFileFinished(filename);
    }
}

void DownloadListWidget::onInstallFileError(const QString& repoId, const QString& filename, const QString& error)
{
    if (auto* card = m_cards.value(repoId, nullptr)) {
        card->markFileError(filename, error);
    }
}

void DownloadListWidget::onInstallGroupFinished(const QString& repoId, bool success, const QString& msg)
{
    if (auto* card = m_cards.value(repoId, nullptr)) {
        card->updateGroupFinished(success, msg);
    }
    // 可选:安装成功几秒后自动从列表移除卡片,这里先保留展示"已完成"状态,
    // 如果想自动清理,可以用 QTimer::singleShot 延迟调用 removeCard(repoId)
}