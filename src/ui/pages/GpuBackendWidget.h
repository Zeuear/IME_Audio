#pragma once
#include <QWidget>
#include "utils/DownloadText.h"

class GpuRuntime;
class QLabel;
class QPushButton;
class QProgressBar;
class QVBoxLayout;


class GpuBackendWidget : public QWidget {
    Q_OBJECT
public:
    enum class GpuStatus {
        Detecting,
        NoGpu,
        GpuAvailable,
        Downloading,
        Installing,
        Ready,
        Failed
    };

    enum class ComputeMode {
        CPU,
        CUDA
    };

    explicit GpuBackendWidget(QWidget *parent = nullptr);
    ~GpuBackendWidget();

    void setGpuRuntime(GpuRuntime* runtime);

    void redetect();
    bool isGpuAccelerationReady() const;

    // 设置模式但不发出用户操作信号。
    void setComputeMode(ComputeMode mode);

protected:
    virtual void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

signals:
    void statusChanged(GpuStatus status);
    void detectFinished(bool result);
    void computeModeChanged(ComputeMode mode);

private slots:
    void onDownloadClicked();
    void onDetectionFinished();

    void onInstallerStatusChanged();
    void onInstallerFinished(bool success, const QString& msg);
    void onModeSwitchClicked();  

private:
    void retranslateUi();
    void setupUi();
    void applyStatus(GpuStatus status);
    void syncFromRuntime();
    ComputeMode currentComputeMode() const;
    void updateModeSwitchUi();
    QString backendUnavailableText() const;

private:
    GpuStatus m_status = GpuStatus::Detecting;
    QString m_backendPath;
    GpuRuntime* m_gpuRuntime = nullptr;

    QWidget   *m_cpuCard = nullptr;
    QLabel    *m_cpuIconLabel = nullptr;
    QLabel    *m_cpuTitleLabel = nullptr;
    QLabel    *m_cpuSubLabel = nullptr;

    QLabel    *m_cudaSectionTitle = nullptr;
    QLabel    *m_cudaSectionDesc = nullptr;

    QWidget   *m_downloadRow = nullptr;
    QLabel    *m_downloadTitleLabel = nullptr;
    QLabel    *m_downloadSizeLabel = nullptr;
    QPushButton *m_downloadButton = nullptr;
    QProgressBar *m_progressBar = nullptr;
    DownloadGroupProgress m_downloadProgress;
    QString m_lastDownloadError;

    QWidget* m_modeSwitchRow = nullptr;
    QLabel* m_modeSwitchLabel = nullptr;
    QPushButton* m_modeSwitchButton = nullptr;

    QLabel* m_sherpaLabel = nullptr;
    QPushButton* m_sherpaButton = nullptr;

    QVBoxLayout *m_rootLayout = nullptr;
};