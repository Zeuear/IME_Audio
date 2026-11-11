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
#include "utils/Logger.h"
#include "installers/CudaInstaller.h"
#include "sherpa/GpuRuntime.h"

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

    connect(m_cudaInstaller, &CudaInstaller::installGroupStarted, this,
            [this](const QString&, const QString&, int totalFiles) {
                m_downloadProgress.start(totalFiles);
                m_lastDownloadError.clear();
                m_progressBar->setValue(0);
            });
    connect(m_cudaInstaller, &CudaInstaller::installFileProgress, this,
            [this](const QString&, const QString& filename, qint64, qint64, int overallPercent) {
                m_downloadProgress.update(filename, overallPercent);
                m_progressBar->setValue(m_downloadProgress.percent());
                if (m_status == GpuStatus::Downloading) m_downloadSizeLabel->setText(m_downloadProgress.line());
            });
    connect(m_cudaInstaller, &CudaInstaller::installFileFinished, this,
            [this]() { m_downloadProgress.fileFinished(); });
    connect(m_cudaInstaller, &CudaInstaller::installFileError, this,
            [this](const QString&, const QString&, const QString& error) { m_lastDownloadError = error; });
    // 下载失败时安装器只发 installGroupFinished(false)，不发 installFinished：不接这个信号，
    // 界面会一直停在"下载中，取消"，用户看不到失败也点不了重试
    connect(m_cudaInstaller, &CudaInstaller::installGroupFinished, this,
            [this](const QString&, bool success, const QString& message) {
                if (success) return;
                applyStatus(GpuStatus::Failed);
                m_downloadSizeLabel->setText(m_lastDownloadError.isEmpty() ? message : message + QStringLiteral(": ") + m_lastDownloadError);
            });

}

void GpuBackendWidget::setGpuRuntime(GpuRuntime* runtime)
{
    m_gpuRuntime = runtime;
    if (!m_gpuRuntime) return;

    connect(m_gpuRuntime, &GpuRuntime::stateChanged, this, &GpuBackendWidget::syncFromRuntime);
    connect(m_gpuRuntime, &GpuRuntime::detectionFinished, this, &GpuBackendWidget::onDetectionFinished);
    redetect();
}

void GpuBackendWidget::setupUi()
{
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(24, 24, 24, 24);
    m_rootLayout->setSpacing(10);

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

    m_cudaSectionTitle = new QLabel(tr("Inference backend"), this);
    m_cudaSectionTitle->setObjectName("cudaSectionTitle");
    m_cudaSectionDesc = new QLabel(tr("Enable NVIDIA GPU acceleration through downloadable CUDA backend."), this);
    m_cudaSectionDesc->setObjectName("cudaSectionDesc");
    m_cudaSectionDesc->setWordWrap(true);

    m_rootLayout->addSpacing(4);
    m_rootLayout->addWidget(m_cudaSectionTitle);
    m_rootLayout->addWidget(m_cudaSectionDesc);

    auto *divider = new QFrame(this);
    divider->setObjectName("sectionDivider");
    divider->setFrameShape(QFrame::HLine);
    divider->setFixedHeight(1);
    m_rootLayout->addWidget(divider);

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
    m_progressBar = new QProgressBar(m_downloadRow);
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    m_progressBar->hide();
    downloadTextCol->addWidget(m_progressBar);

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
    m_progressBar->setVisible(status == GpuStatus::Downloading);

    switch (status) {
    case GpuStatus::Detecting:
        m_cpuTitleLabel->setText(QString(tr("Detecting hardware…")));
        m_cpuSubLabel->setText(QString(tr("Please wait")));
        m_downloadRow->setVisible(false);
        break;

    case GpuStatus::NoGpu:
        m_cpuIconLabel->setText("\u25A3");
        m_cpuTitleLabel->setText(tr("CPU only"));
        m_cpuSubLabel->setText(cudaPlatformSpec().detectable
            ? tr("Can't detect GPU acceleration")
            : tr("GPU acceleration unavailable on this platform"));
        m_downloadRow->setVisible(false);
        m_cudaSectionDesc->setText(backendUnavailableText());
        break;

    case GpuStatus::GpuAvailable:
        m_cpuIconLabel->setText("\u26A1");
        m_cpuTitleLabel->setText(tr("Detected GPU：%1").arg(m_gpuRuntime ? m_gpuRuntime->detection().gpuName : QString()));
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
        m_cpuTitleLabel->setText(QString(tr("Detected GPU：%1")).arg(m_gpuRuntime ? m_gpuRuntime->detection().gpuName : QString()));
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
    if (currentComputeMode() == ComputeMode::CUDA) {
        m_modeSwitchLabel->setText(tr("Current Using: GPU (CUDA)"));
        m_modeSwitchButton->setText(tr("Exchange CPU"));
    }
    else {
        m_modeSwitchLabel->setText(tr("Current Using: CPU"));
        m_modeSwitchButton->setText(tr("Exchange GPU (CUDA)"));
    }
}

GpuBackendWidget::ComputeMode GpuBackendWidget::currentComputeMode() const
{
    return (m_gpuRuntime && m_gpuRuntime->requestedGpu()) ? ComputeMode::CUDA : ComputeMode::CPU;
}

void GpuBackendWidget::onModeSwitchClicked()
{
    setComputeMode(currentComputeMode() == ComputeMode::CUDA ? ComputeMode::CPU : ComputeMode::CUDA);
    emit computeModeChanged(currentComputeMode());
}

void GpuBackendWidget::setComputeMode(ComputeMode mode)
{
    if (!m_gpuRuntime) return;
    // 平台没有 CUDA / 后端没装好时由 GpuRuntime 锁死 CPU，控件不自己判断
    m_gpuRuntime->setRequestedGpu(mode == ComputeMode::CUDA);
    updateModeSwitchUi();
}


bool GpuBackendWidget::isGpuAccelerationReady() const
{
    return m_status == GpuStatus::Ready && currentComputeMode() == ComputeMode::CUDA;
}

void GpuBackendWidget::redetect()
{
    if (!m_gpuRuntime) return;
    applyStatus(GpuStatus::Detecting);
    if (m_cudaInstaller) m_cudaInstaller->setEnvironment();
    m_gpuRuntime->redetect();
}

void GpuBackendWidget::syncFromRuntime()
{
    updateModeSwitchUi();
    if (m_gpuRuntime && m_gpuRuntime->lastLoadMismatch()) {
        LOG_WARN(QString("GPU requested but load actually used %1 (%2)")
                     .arg(m_gpuRuntime->actualProvider() == GpuProvider::Cpu ? "cpu" : "unknown",
                          m_gpuRuntime->actualRepoId()));
    }
}

void GpuBackendWidget::onDetectionFinished()
{
    if (!m_gpuRuntime) return;

    const GpuDetectionResult& result = m_gpuRuntime->detection();
    LOG_INFO(QString("GPU detection: gpu=%1 (%2), cudart=%3 (%4), cudnn=%5, ortCudaProvider=%6, ready=%7, mode=%8, reason=%9")
                 .arg(result.hasNvidiaGpu).arg(result.gpuName).arg(result.hasCudaRuntime).arg(result.cudaRuntimeVersion)
                 .arg(result.hasCudnn).arg(result.hasOrtCudaProvider).arg(result.isFullyReady)
                 .arg(currentComputeMode() == ComputeMode::CUDA ? "CUDA" : "CPU").arg(result.failReason));

    if (!result.hasNvidiaGpu) {
        applyStatus(GpuStatus::NoGpu);
    }
    else if (result.isFullyReady) {
        applyStatus(GpuStatus::Ready);
    }
    else {
        applyStatus(GpuStatus::GpuAvailable);
        m_cudaSectionDesc->setText(result.failReason);
    }
    emit detectFinished(m_gpuRuntime->requestedGpu());
}

void GpuBackendWidget::onDownloadClicked()
{
    if (!m_cudaInstaller || !m_gpuRuntime) return;

    if (m_status == GpuStatus::Downloading) {
        m_cudaInstaller->cancelDownload();
        applyStatus(GpuStatus::GpuAvailable);
        return;
    }

    applyStatus(GpuStatus::Downloading);
    m_cudaInstaller->startDownload(m_gpuRuntime->detection());
}

void GpuBackendWidget::onInstallerStatusChanged()
{
    applyStatus(GpuStatus::Installing);
}

void GpuBackendWidget::onInstallerFinished(bool success, const QString& msg)
{
    if (success) {
        m_downloadSizeLabel->setText(msg);
        // 装完不直接假定就绪：重新检测 provider 版本与硬件，结果仍由 GpuRuntime 说了算
        applyStatus(GpuStatus::Detecting);
        if (m_gpuRuntime) {
            if (m_cudaInstaller) m_cudaInstaller->setEnvironment();
            m_gpuRuntime->redetect();
        }
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


QString GpuBackendWidget::backendUnavailableText() const
{
    if (!cudaPlatformSpec().detectable) {
        return tr("This platform runs inference on the CPU. GPU acceleration is not available here.");
    }
    return tr("No supported NVIDIA GPU detected. Current implementation uses CPU for inference.");
}

void GpuBackendWidget::retranslateUi()
{
    m_cudaSectionTitle->setText(tr("Inference backend"));
    m_cudaSectionDesc->setText(cudaPlatformSpec().detectable
        ? tr("Enable NVIDIA GPU acceleration through downloadable CUDA backend.")
        : backendUnavailableText());
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

