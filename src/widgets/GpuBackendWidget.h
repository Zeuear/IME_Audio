#pragma once
#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include "../cuda/CudaInstaller.h"

class CudaInstaller;
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

    void setBackendInstaller(QNetworkAccessManager* nam);

    void redetect();
    bool isGpuAccelerationReady() const;

    ComputeMode currentComputeMode() const { return m_computeMode; }
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
    void onDetectionFinished(const GpuDetectionResult& result);
    void onInstallerStatusChanged();
    void onInstallerProgressUpdated(int percentage);
    void onInstallerFinished(bool success, const QString& msg);
    void onModeSwitchClicked();   // 新增

private:
    void retranslateUi();
    void setupUi();
    void applyStatus(GpuStatus status);
    void detectGpuAsync();
    void updateModeSwitchUi();   

private:
    QFutureWatcher<GpuDetectionResult>* m_detectWatcher = nullptr;

    GpuStatus m_status = GpuStatus::Detecting;
    ComputeMode m_computeMode = ComputeMode::CPU;
    QString m_detectedGpuName;
    QString m_backendPath;

    // CPU 检测卡片
    QWidget   *m_cpuCard = nullptr;
    QLabel    *m_cpuIconLabel = nullptr;
    QLabel    *m_cpuTitleLabel = nullptr;
    QLabel    *m_cpuSubLabel = nullptr;

    // CUDA 后端区域
    QLabel    *m_cudaSectionTitle = nullptr;
    QLabel    *m_cudaSectionDesc = nullptr;

    QWidget   *m_downloadRow = nullptr;
    QLabel    *m_downloadTitleLabel = nullptr;
    QLabel    *m_downloadSizeLabel = nullptr;
    QPushButton *m_downloadButton = nullptr;
    QProgressBar *m_progressBar = nullptr;

    // 计算模式切换行(仅在 Ready 状态下显示)
    QWidget* m_modeSwitchRow = nullptr;
    QLabel* m_modeSwitchLabel = nullptr;
    QPushButton* m_modeSwitchButton = nullptr;

    QVBoxLayout *m_rootLayout = nullptr;
    QScopedPointer<class QFile> m_downloadFile;
    CudaInstaller* m_cudaInstaller = nullptr;
};