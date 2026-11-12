#include "GpuRuntime.h"

#include "installers/CudaInstaller.h"

void GpuRuntime::setInstaller(CudaInstaller* installer)
{
    if (m_installer == installer) return;
    if (m_installer) disconnect(m_installer, nullptr, this, nullptr);
    m_installer = installer;
    if (!m_installer) {
        m_installing = false;
        emit stateChanged();
        return;
    }

    connect(m_installer, &CudaInstaller::installGroupStarted, this, &GpuRuntime::installGroupStarted);
    connect(m_installer, &CudaInstaller::installGroupFinished, this,
            [this](const QString& groupId, bool success, const QString& message) {
                if (!success) m_installing = false;
                emit installGroupFinished(groupId, success, message);
                emit stateChanged();
            });
    connect(m_installer, &CudaInstaller::installFileProgress, this, &GpuRuntime::installFileProgress);
    connect(m_installer, &CudaInstaller::installFileFinished, this, &GpuRuntime::installFileFinished);
    connect(m_installer, &CudaInstaller::installFileError, this, &GpuRuntime::installFileError);
    connect(m_installer, &CudaInstaller::extractStarted, this, &GpuRuntime::extractStarted);
    connect(m_installer, &CudaInstaller::extractProgress, this, &GpuRuntime::extractProgress);
    connect(m_installer, &CudaInstaller::extractFinished, this, &GpuRuntime::extractFinished);
    connect(m_installer, &CudaInstaller::statusChanged, this, &GpuRuntime::statusChanged);
    connect(m_installer, &CudaInstaller::progressUpdated, this, &GpuRuntime::progressUpdated);
    connect(m_installer, &CudaInstaller::installStarted, this, [this]() {
        m_installing = true;
        emit installStarted();
        emit stateChanged();
    });
    connect(m_installer, &CudaInstaller::installFinished, this, [this](bool success, const QString& message) {
        m_installing = false;
        emit installFinished(success, message);
        emit stateChanged();
    });
}

void GpuRuntime::setEnvironment()
{
    if (m_installer) m_installer->setEnvironment();
}

void GpuRuntime::startInstall()
{
    if (!m_installer) {
        const QString message = QStringLiteral("GPU 安装服务未配置");
        emit installGroupFinished(QStringLiteral("cuda_install"), false, message);
        return;
    }
    if (m_installing) return;
    m_installing = true;
    emit stateChanged();
    m_installer->startDownload(m_detection);
}

void GpuRuntime::cancelInstall()
{
    if (!m_installer || !m_installing) return;
    m_installer->cancelDownload();
    // 取消不会有任何完成信号回来，这里自己复位，否则再也点不动下载
    m_installing = false;
    emit stateChanged();
}

void GpuRuntime::redetect()
{
    m_detectionDone = false;
    m_detection = {};
    setEnvironment();
    detectAsync();
}
