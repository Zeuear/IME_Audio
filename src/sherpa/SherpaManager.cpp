#include "SherpaManager.h"
#include <QFileInfo>
#include <QWebSocket>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QtConcurrent> 
#include "../utils/Logger.h"


SherpaManager::SherpaManager(QObject* parent)
    : QObject(parent), m_isLoaded(false)
{
    m_workerThread = QThread::create([this]() { workerLoop(); });
    m_workerThread->start();
}

SherpaManager::~SherpaManager(){
    shutdown();
}

void SherpaManager::shutdown()
{
    {
        QMutexLocker locker(&m_queueMutex);
        m_stopWorker = true;
        m_queueNotEmpty.wakeAll(); 
    }
    m_workerThread->wait(); 
    delete m_workerThread;
    m_workerThread = nullptr;
}

bool SherpaManager::isModelLoaded() const
{
    return m_isLoaded;
}

bool SherpaManager::isBusy() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_busyFlag;
}

int SherpaManager::pendingCount() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_queue.size();
}

void SherpaManager::unloadModel()
{
    {
        QMutexLocker queueLocker(&m_queueMutex);
        m_queue.clear();
    }
    emit queueSizeChanged(0);
    while (isBusy()) {
        QThread::msleep(5);
    }

    {
        QMutexLocker locker(&m_recognizerMutex);
        m_offlineRecognizer.reset(); 
        m_onlineRecognizer.reset();
        m_kind = RecognizerKind::None;
        m_isLoaded = false;
        m_currentRepoId.clear();
    }
    qDebug() << "Sherpa model unloaded, memory released.";
}

void SherpaManager::loadModel(const QString& repoId, int numThreads, bool useGpu)
{
    QMutexLocker locker(&m_recognizerMutex);

    if (m_isLoaded && m_currentRepoId == repoId) {
        LOG_INFO(QString("Model %1 already loaded, reusing cached recognizer.").arg(repoId));
        return;
    }

    m_isLoaded = false;
    m_offlineRecognizer.reset();
    m_onlineRecognizer.reset();
    m_kind = RecognizerKind::None;
    m_currentRepoId.clear();

	auto result = ModelRegistry::GetConfig(repoId, numThreads, useGpu);
	m_isLoaded = result.isLoaded;
    if (!result.isLoaded) {
        LOG_ERROR("Model or tokens file does not exist!");
        return;
    }

    switch (result.kind)
    {
	case RecognizerKind::Offline:
        if (auto* pOffline = std::get_if<std::unique_ptr<sherpa_onnx::cxx::OfflineRecognizer>>(&result.recognizer)) {
            m_offlineRecognizer = std::move(*pOffline);
            m_kind = RecognizerKind::Offline;
        }
        else {
            LOG_ERROR("Variant content does not match RecognizerKind::Offline!");
        }
		break;
	case RecognizerKind::Online:
        if (auto* pOnline = std::get_if<std::unique_ptr<sherpa_onnx::cxx::OnlineRecognizer>>(&result.recognizer)) {
            m_onlineRecognizer = std::move(*pOnline);
            m_kind = RecognizerKind::Online;
        }
        else {
            LOG_ERROR("Variant content does not match RecognizerKind::Online!");
        }
		break;
    default:
        break;
    }

    if (m_isLoaded) {
        m_currentRepoId = repoId;
    }
}

bool SherpaManager::transcribeSync(const QByteArray& pcmData, int sampleRate, QString* outText, QString* outError)
{
    QMutexLocker locker(&m_recognizerMutex);
    if (!m_isLoaded || m_kind == RecognizerKind::None) {
        if (outError) *outError = QStringLiteral("模型尚未加载初始化");
        return false;
    }

    if (pcmData.isEmpty()) {
        if (outError) *outError = QStringLiteral("音频文件不存在");
        return false;
    }

    const int16_t* rawSamples = reinterpret_cast<const int16_t*>(pcmData.constData());
    const int count = pcmData.size() / 2;
    if (count == 0) return false;

    std::vector<float> samples(count);
    for (int i = 0; i < count; ++i)
        samples[i] = rawSamples[i] / 32768.0f;

    switch (m_kind) {
    case RecognizerKind::Offline:
        return transcribeOffline(samples, sampleRate, outText, outError);
    case RecognizerKind::Online:
        return transcribeOnline(samples, sampleRate, outText, outError);
    default:
        if (outError) *outError = QStringLiteral("未知的识别器类型");
        return false;
    }
}

bool SherpaManager::transcribeOffline(const std::vector<float>& samples, int sampleRate, QString* outText, QString* outError)
{
    if (!m_offlineRecognizer) {
        if (outError) *outError = QStringLiteral("离线识别器未初始化");
        return false;
    }

    try {
        auto stream = m_offlineRecognizer->CreateStream();
        stream.AcceptWaveform(sampleRate, samples.data(), static_cast<int32_t>(samples.size()));

        m_offlineRecognizer->Decode(&stream);
        auto result = m_offlineRecognizer->GetResult(&stream);

        if (outText) *outText = QString::fromStdString(result.text).trimmed();
        return true;
    }
    catch (const std::exception& e) {
        if (outError) *outError = QString::fromLocal8Bit(e.what());
        return false;
    }
}

bool SherpaManager::transcribeOnline(const std::vector<float>& samples, int sampleRate, QString* outText, QString* outError)
{
    if (!m_onlineRecognizer) {
        if (outError) *outError = QStringLiteral("在线识别器未初始化");
        return false;
    }

    try {
        auto stream = m_onlineRecognizer->CreateStream();

        const int32_t chunkSize = static_cast<int32_t>(0.1 * sampleRate);
        int32_t offset = 0;
        const int32_t total = static_cast<int32_t>(samples.size());

        while (offset < total) {
            int32_t n = std::min(chunkSize, total - offset);
            stream.AcceptWaveform(sampleRate, samples.data() + offset, n);
            offset += n;

            while (m_onlineRecognizer->IsReady(&stream)) {
                m_onlineRecognizer->Decode(&stream);
            }
        }

        stream.InputFinished();
        while (m_onlineRecognizer->IsReady(&stream)) {
            m_onlineRecognizer->Decode(&stream);
        }

        auto result = m_onlineRecognizer->GetResult(&stream);
        if (outText) *outText = QString::fromStdString(result.text).trimmed();
        return true;
    }
    catch (const std::exception& e) {
        if (outError) *outError = QString::fromLocal8Bit(e.what());
        return false;
    }
}

void SherpaManager::transcribeAsync(const QByteArray& pcmData, int sampleRate)
{
    if (pcmData.isEmpty()) return;
    if (!m_isLoaded) {
        return;
    }

    {
        QMutexLocker locker(&m_queueMutex);
        m_queue.enqueue({ pcmData, sampleRate });
        qDebug() << "[enqueue]" << QThread::currentThreadId() << "queue size=" << m_queue.size();
        m_queueNotEmpty.wakeOne(); // 唤醒可能正在休眠等待任务的 worker 线程
    }
    emit queueSizeChanged(pendingCount());
}

void SherpaManager::workerLoop()
{
    forever{
        PendingUtterance task;

        {
            QMutexLocker locker(&m_queueMutex);
            while (m_queue.isEmpty() && !m_stopWorker) {
                qDebug() << "[worker] waiting..." << QThread::currentThreadId();
                m_queueNotEmpty.wait(&m_queueMutex);
                qDebug() << "[worker] woke up, queue size=" << m_queue.size();
            }
            if (m_stopWorker && m_queue.isEmpty()) {
                return;
            }
            task = m_queue.dequeue();
            m_busyFlag = true;
        }
        emit queueSizeChanged(pendingCount());

        QString text, error;
        bool success = transcribeSync(task.pcmData, task.sampleRate, &text, &error);
        
        {
            QMutexLocker locker(&m_queueMutex);
            m_busyFlag = false;
        }
        emit utteranceTranscribed(success, text, error);
    }
}


SherpaInstaller::SherpaInstaller(QNetworkAccessManager* nam, QObject* parent) : QObject(parent)
{
    connect(this, &SherpaInstaller::installationProgress, 
        this, [this](const QString& msg) {  LOG_INFO(msg); });
    connect(this, &SherpaInstaller::installationFinished, 
        this, [this](bool ok, const QString& msg) { ok ? LOG_INFO(msg) : LOG_ERROR(msg); });
    connect(this, &SherpaInstaller::installFileError, 
        this, [this](const QString&, const QString&, const QString& error) { LOG_ERROR(error); });
    connect(this, &SherpaInstaller::installGroupFinished, 
        this, [this](const QString&, bool success, const QString& msg) { success ? LOG_INFO(msg) : LOG_ERROR(msg);});

    m_downloadManager = &DownloadManager::instance(nam);

    connect(m_downloadManager, &DownloadManager::groupFileProgress, this, &SherpaInstaller::onGroupFileProgress);
    connect(m_downloadManager, &DownloadManager::groupFileFinished, this, &SherpaInstaller::onGroupFileFinished);
    connect(m_downloadManager, &DownloadManager::groupFileError, this, &SherpaInstaller::onGroupFileError);
    connect(m_downloadManager, &DownloadManager::groupFinished, this, &SherpaInstaller::onGroupFinished);
}

bool SherpaInstaller::isInstalling(const QString& repoId) const
{
    return m_activeManifests.contains(repoId) || !m_downloadManager->tasksInGroup(repoId).isEmpty();
}

bool SherpaInstaller::isInstalled(const QString& repoId) const
{
	QString repoName = repoId.split("/").last();
	QString modelPath = ModelConfigFactory::getSherpaModel() + "/" + repoName;
	return QFile::exists(modelPath);
}

void SherpaInstaller::installModel(const QString& repoId)
{
    QString repoName = repoId.split("/").last();
    QString modelPath = ModelConfigFactory::getSherpaModel() + "/" + repoName;
    if (QFile::exists(modelPath)) {
        LOG_INFO(QString("%1 is exist.").arg(repoId));
        emit installGroupFinished(repoId, true, tr("Download Complete"));
        return;
    }

    ModelInstallManifest manifest = ModelRegistry::BuildManifest(repoId);

    if (!manifest.archiveUrl.isEmpty()) {
        m_activeManifests[repoId] = manifest;
        m_downloadManager->addGroupTask(repoId, manifest.displayName, QUrl(manifest.archiveUrl), manifest.archiveLocalPath);
        emit installGroupStarted(repoId, manifest.displayName, 1);
        return;
    }

    for (const auto& f : manifest.files) {
        m_downloadManager->addGroupTask(repoId, manifest.displayName, f.sourceUrl, f.localPath);
    }
    emit installGroupStarted(repoId, manifest.displayName, static_cast<int>(manifest.files.size()));
}


void SherpaInstaller::uninstallAll()
{
    QString sherpaModelRoot = ModelConfigFactory::getSherpaModel();
    QDir dir(sherpaModelRoot);
    if (dir.exists() && dir.removeRecursively()) {
        emit installationFinished(true, "Sherpa Uninstall Complete");
    }
    else {
        emit installationFinished(false, "Uninstall Error");
    }
}

void SherpaInstaller::onGroupFileProgress(const QString& groupId, const QString&, const QString& filename,
    qint64 received, qint64 total, int overallPercent)
{
    emit installFileProgress(groupId, filename, received, total, overallPercent);
}

void SherpaInstaller::onGroupFileFinished(const QString& groupId, const QString&, const QString& filename)
{
    emit installFileFinished(groupId, filename);
}

void SherpaInstaller::onGroupFileError(const QString& groupId, const QString&, const QString& filename, const QString& error)
{
    emit installFileError(groupId, filename, error);
}

void SherpaInstaller::onGroupFinished(const QString& groupId, bool success)
{
    if (!success) {
        emit installGroupFinished(groupId, false, tr("Download Error"));
        return;
    }

    auto it = m_activeManifests.find(groupId);
    if (it == m_activeManifests.end()) {
        emit installGroupFinished(groupId, true, tr("Download Complete"));
        return;
    }

    const ModelInstallManifest& manifest = it.value();
    emit installationProgress(tr("正在解压 %1 ...").arg(manifest.displayName));

    QString root = ModelConfigFactory::getSherpaRoot();
    if (!extractTarBz2(manifest.archiveLocalPath, root)) {
        emit installGroupFinished(groupId, false, tr("Extract Error"));
        return;
    }
    QString extractedDir = root + "/" + manifest.archiveExtractedDirName;
    if (!moveDirContents(extractedDir, manifest.archiveTargetDir)) {
        emit installGroupFinished(groupId, false, tr("Move Error"));
        return;
    }
    QDir(extractedDir).removeRecursively();
    QFile::remove(manifest.archiveLocalPath);

    m_activeManifests.remove(groupId);
    emit installGroupFinished(groupId, true, tr("Download Complete"));
}

bool SherpaInstaller::extractTarBz2(const QString& archivePath, const QString& destDir) const
{
    if (!QFile::exists(archivePath))
        return false;

    QDir().mkpath(destDir);

    QProcess proc;
    proc.setProgram("tar");
    proc.setArguments({ "-xjf", QDir::toNativeSeparators(archivePath), "-C", QDir::toNativeSeparators(destDir) });
    proc.start();
    if (!proc.waitForFinished(5 * 60 * 1000))
        return false;

    return proc.exitCode() == 0;
}

bool SherpaInstaller::moveDirContents(const QString& srcDir, const QString& destDir) const
{
    QDir().mkpath(destDir);
    QDir src(srcDir);
    if (!src.exists()) return false;

    QDirIterator it(srcDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString srcFile = it.next();
        QString relPath = src.relativeFilePath(srcFile);
        QString destFile = destDir + "/" + relPath;

        QDir().mkpath(QFileInfo(destFile).absolutePath());
        if (QFile::exists(destFile)) {
            QFile::remove(destFile);
        }
        if (!QFile::rename(srcFile, destFile)) {
            return false;
        }
    }
    return true;
}