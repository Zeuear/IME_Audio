#include "GpuBackendWidget.h"

#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QNetworkRequest>
#include <QTimer>
#include <QDesktopServices>
#include <QMessageBox>
#include <QLibrary>
#include <QStyleOption>
#include <QPainter>
#include "../utils/Logger.h"

GpuBackendWidget::GpuBackendWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

GpuBackendWidget::~GpuBackendWidget()
{}

void GpuBackendWidget::setBackendInstaller(CudaInstaller* cudaInstaller)
{
    m_cudaInstaller = cudaInstaller;
    connect(m_cudaInstaller, &CudaInstaller::installStarted, this, &GpuBackendWidget::onInstallerStatusChanged);
    connect(m_cudaInstaller, &CudaInstaller::installFinished, this, &GpuBackendWidget::onInstallerFinished);

    detectGpuAsync();
}

void GpuBackendWidget::setupUi()
{
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(24, 24, 24, 24);
    m_rootLayout->setSpacing(10);

    // 顶部状态卡片(仅CPU / 检测到GPU)
    m_cpuCard = new QFrame(this);
    m_cpuCard->setObjectName("statusCard");
    auto *cardLayout = new QHBoxLayout(m_cpuCard);
    cardLayout->setContentsMargins(18, 16, 18, 16);
    cardLayout->setSpacing(10);

    m_cpuIconLabel = new QLabel(m_cpuCard);
    m_cpuIconLabel->setObjectName("cpuIconLabel");
    m_cpuIconLabel->setFixedSize(28, 28);
    m_cpuIconLabel->setAlignment(Qt::AlignCenter);
    m_cpuIconLabel->setText("\u2699"); 

    auto *textCol = new QVBoxLayout();
    textCol->setSpacing(4);
    m_cpuTitleLabel = new QLabel(m_cpuCard);
    m_cpuTitleLabel->setObjectName("cpuTitleLabel");
    m_cpuSubLabel = new QLabel(m_cpuCard);
    m_cpuSubLabel->setObjectName("cpuSubLabel");
    textCol->addWidget(m_cpuTitleLabel);
    textCol->addWidget(m_cpuSubLabel);

    cardLayout->addWidget(m_cpuIconLabel);
    cardLayout->addLayout(textCol, 1);
    m_rootLayout->addWidget(m_cpuCard);

    // CUDA 后端标题区域
    m_cudaSectionTitle = new QLabel(tr("CUDA backend"), this);
    m_cudaSectionTitle->setObjectName("cudaSectionTitle");
    m_cudaSectionDesc = new QLabel(tr("Enable NVIDIA GPU acceleration through downloadable CUDA backend."), this);
    m_cudaSectionDesc->setObjectName("cudaSectionDesc");
    m_cudaSectionDesc->setWordWrap(true);

    m_rootLayout->addSpacing(4);
    m_rootLayout->addWidget(m_cudaSectionTitle);
    m_rootLayout->addWidget(m_cudaSectionDesc);

    // 分隔线
    auto *divider = new QFrame(this);
    divider->setObjectName("sectionDivider");
    divider->setFrameShape(QFrame::HLine);
    divider->setFixedHeight(1);
    m_rootLayout->addWidget(divider);

    // 下载行
    m_downloadRow = new QWidget(this);
    m_downloadRow->setAttribute(Qt::WidgetAttribute::WA_TranslucentBackground);
    auto *downloadLayout = new QHBoxLayout(m_downloadRow);
    downloadLayout->setContentsMargins(0, 8, 0, 8);
    downloadLayout->setSpacing(10);

    auto *downloadTextCol = new QVBoxLayout();
    downloadTextCol->setSpacing(4);
    m_downloadTitleLabel = new QLabel(QString(tr("Download CUDA Backend")), m_downloadRow);
    m_downloadTitleLabel->setObjectName("downloadTitleLabel");
    m_downloadSizeLabel = new QLabel(QString(tr("Approximately 2.4 GB. Requires NVIDIA GPU with CUDA support.")), m_downloadRow);
    m_downloadSizeLabel->setObjectName("downloadSizeLabel");
    m_downloadSizeLabel->setWordWrap(true);
    downloadTextCol->addWidget(m_downloadTitleLabel);
    downloadTextCol->addWidget(m_downloadSizeLabel);

    m_downloadButton = new QPushButton(m_downloadRow);
    m_downloadButton->setObjectName("downloadButton");
    m_downloadButton->setFixedSize(120, 40);
    m_downloadButton->setCursor(Qt::PointingHandCursor);

    downloadLayout->addLayout(downloadTextCol, 1);
    downloadLayout->addWidget(m_downloadButton, 0, Qt::AlignVCenter);

    m_rootLayout->addWidget(m_downloadRow);
    m_rootLayout->addStretch(1);

    connect(m_downloadButton, &QPushButton::clicked, this, &GpuBackendWidget::onDownloadClicked);

    m_modeSwitchRow = new QWidget(this);
    m_modeSwitchRow->setAttribute(Qt::WidgetAttribute::WA_TranslucentBackground);

    auto* modeLayout = new QHBoxLayout(m_modeSwitchRow);
    modeLayout->setContentsMargins(0, 8, 0, 8);
    modeLayout->setSpacing(10);

    m_modeSwitchLabel = new QLabel(m_modeSwitchRow);
    m_modeSwitchLabel->setObjectName("modeSwitchLabel");

    m_modeSwitchButton = new QPushButton(m_modeSwitchRow);
    m_modeSwitchButton->setObjectName("modeSwitchButton");
    m_modeSwitchButton->setFixedSize(200, 36);
    m_modeSwitchButton->setCursor(Qt::PointingHandCursor);

    modeLayout->addWidget(m_modeSwitchLabel, 1);
    modeLayout->addWidget(m_modeSwitchButton, 0, Qt::AlignVCenter);

    m_modeSwitchRow->setVisible(false);
    m_rootLayout->addWidget(m_modeSwitchRow);

    connect(m_modeSwitchButton, &QPushButton::clicked, this, &GpuBackendWidget::onModeSwitchClicked);

    m_rootLayout->addStretch(1);
}

void GpuBackendWidget::applyStatus(GpuStatus status)
{
    m_status = status;
    m_modeSwitchRow->setVisible(false);

    switch (status) {
    case GpuStatus::Detecting:
        m_cpuTitleLabel->setText(QString(tr("Detecting hardware…")));
        m_cpuSubLabel->setText(QString(tr("Please wait")));
        m_downloadRow->setVisible(false);
        break;

    case GpuStatus::NoGpu:
        m_cpuIconLabel->setText("\u25A3");
        m_cpuTitleLabel->setText("Only CPU");
        m_cpuSubLabel->setText("Can't Detect GPU Acceleration");
        m_downloadRow->setVisible(false); 
        m_cudaSectionDesc->setText(QString(tr("No supported NVIDIA GPU detected. Current implementation uses CPU for inference.")));
        break;

    case GpuStatus::GpuAvailable:
        m_cpuIconLabel->setText("\u26A1");
        m_cpuTitleLabel->setText(tr("Detected GPU：%1").arg(m_detectedGpuName));
        m_cpuSubLabel->setText(tr("Can download CUDA backend to enable acceleration"));
        m_cudaSectionDesc->setText(tr("Through downloadable CUDA backend Enable NVIDIA GPU acceleration."));
        m_downloadRow->setVisible(true);
        m_downloadTitleLabel->setText(tr("Download CUDA Backend"));
        m_downloadSizeLabel->setText(tr("Approximately 3 GB. Requires NVIDIA GPU with CUDA support."));
        m_downloadButton->setText(tr("Download"));
        m_downloadButton->setEnabled(true);
        break;

    case GpuStatus::Downloading:
        m_downloadRow->setVisible(true);
        m_downloadTitleLabel->setText(QString(tr("Downloading CUDA offline package…")));
        m_downloadButton->setText(QString(tr("Cancel")));
        m_downloadButton->setEnabled(true);
        break;

    case GpuStatus::Installing:
        m_downloadRow->setVisible(true);
        m_downloadTitleLabel->setText(QString(tr("Installing CUDA…")));
        m_downloadButton->setText(QString(tr("Installing…")));
        m_downloadButton->setEnabled(false); 
        break;

    case GpuStatus::Ready:
        m_cpuTitleLabel->setText(QString(tr("Detected GPU：%1")).arg(m_detectedGpuName));
        m_cpuSubLabel->setText(QString(tr("CUDA backend is ready")));
        m_downloadButton->setText(QString(tr("Downloaded")));
        m_downloadButton->setEnabled(false);
        m_downloadRow->setVisible(false);
        m_modeSwitchRow->setVisible(true);
        updateModeSwitchUi();
        break;

    case GpuStatus::Failed:
        m_downloadRow->setVisible(true);
        m_downloadButton->setText(QString(tr("Retry")));
        m_downloadButton->setEnabled(true);
        break;
    }
    emit statusChanged(status);
}

void GpuBackendWidget::updateModeSwitchUi()
{
    if (m_computeMode == ComputeMode::CUDA) {
        m_modeSwitchLabel->setText(tr("Current Using: GPU (CUDA)"));
        m_modeSwitchButton->setText(tr("Exchange CPU"));
    }
    else {
        m_modeSwitchLabel->setText(tr("Current Using: CPU"));
        m_modeSwitchButton->setText(tr("Exchange GPU (CUDA)"));
    }
}

void GpuBackendWidget::onModeSwitchClicked()
{
    setComputeMode(m_computeMode == ComputeMode::CUDA ? ComputeMode::CPU : ComputeMode::CUDA);
}

void GpuBackendWidget::setComputeMode(ComputeMode mode)
{
    m_computeMode = mode;
    updateModeSwitchUi();

    emit computeModeChanged(mode);
}


bool GpuBackendWidget::isGpuAccelerationReady() const
{
    return m_status == GpuStatus::Ready && m_computeMode == ComputeMode::CUDA;
}

void GpuBackendWidget::redetect()
{
    applyStatus(GpuStatus::Detecting);
    detectGpuAsync();
}

void GpuBackendWidget::detectGpuAsync()
{
    m_cudaInstaller->setEnvironment();
    applyStatus(GpuStatus::Detecting);
    if (m_detectWatcher) {
        m_detectWatcher->cancel();
        m_detectWatcher->deleteLater();
    }

    m_detectWatcher = new QFutureWatcher<GpuDetectionResult>(this);
    connect(m_detectWatcher, &QFutureWatcher<GpuDetectionResult>::finished, this, [this]() {
        GpuDetectionResult result = m_detectWatcher->result();
        onDetectionFinished(result);
    });

    QFuture<GpuDetectionResult> future = QtConcurrent::run([]() {
        return CudaInstaller::detectGpuEnvironment(true);
    });

    m_detectWatcher->setFuture(future);
}

void GpuBackendWidget::onDetectionFinished(const GpuDetectionResult& result)
{
    m_detail = result;
    if (!result.hasNvidiaGpu) {
        m_computeMode = ComputeMode::CPU;
        applyStatus(GpuStatus::NoGpu);
        emit detectFinished(false);
        return;
    }

    m_detectedGpuName = result.gpuName;

    if (result.isFullyReady) {
        applyStatus(GpuStatus::Ready);
        emit detectFinished(m_computeMode == ComputeMode::CUDA);
    }
    else {
        m_computeMode = ComputeMode::CPU;
        applyStatus(GpuStatus::GpuAvailable);
        m_cudaSectionDesc->setText(result.failReason);
        emit detectFinished(false);
    }
}

void GpuBackendWidget::onDownloadClicked()
{
    if (m_status == GpuStatus::Downloading) {
        m_cudaInstaller->cancelDownload();
        applyStatus(GpuStatus::GpuAvailable);
        return;
    }

    if (m_status == GpuStatus::Failed) {
        applyStatus(GpuStatus::Downloading);
        m_cudaInstaller->startDownload(m_detail);
        return;
    }

    applyStatus(GpuStatus::Downloading);
    m_cudaInstaller->startDownload(m_detail);
}

void GpuBackendWidget::onInstallerStatusChanged()
{
    applyStatus(GpuStatus::Installing);
}

void GpuBackendWidget::onInstallerFinished(bool success, const QString& msg)
{
    if (success) {
        applyStatus(GpuStatus::Ready);
        m_downloadSizeLabel->setText(msg); 
    }
    else {
        applyStatus(GpuStatus::Failed);
        m_downloadSizeLabel->setText(msg); 
    }
}

void GpuBackendWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        this->retranslateUi();
    }
    QWidget::changeEvent(event);
}


void GpuBackendWidget::retranslateUi()
{
    m_cudaSectionTitle->setText(tr("CUDA backend"));
    m_cudaSectionDesc->setText(tr("Enable NVIDIA GPU acceleration through downloadable CUDA backend."));
    updateModeSwitchUi();
    applyStatus(m_status);
}

void GpuBackendWidget::paintEvent(QPaintEvent* event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}