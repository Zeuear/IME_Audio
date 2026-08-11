#include "SherpaManager.h"
#include <QFileInfo>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <QtConcurrent> 
#include "../utils/Logger.h"
#include "../utils/ExtractTool.h"
#include "../ConfigManager.h"



SherpaManager::SherpaManager(QObject* parent)
    : ISherpaModel(parent), m_isLoaded(false)
{
    m_punctuator = new SherpaPunctuator(this);
    m_workerThread = QThread::create([this]() { workerLoop(); });
    m_workerThread->start();

    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(kIdleUnloadMs);
    connect(m_idleTimer, &QTimer::timeout, this, [this]() {
        LOG_INFO("Model idle over 30 minutes, auto-unloaded to free memory");
        unloadModel();
    });
}

SherpaManager::~SherpaManager(){
    unloadModel();
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
    return m_busyFlag;
}

int SherpaManager::pendingCount() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_queue.size();
}

void SherpaManager::unloadModel()
{
    if (m_idleTimer) m_idleTimer->stop();
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

    m_punctuator->unload();
    LOG_DEBUG("Sherpa model unloaded, memory released.");
    LOG_INFO("Unload succeeded");
}

bool SherpaManager::reloadModel(const AppConfig& config) {
    QString repoId = ModelRegistry::FindByDisplayName(
        config.sherpa.languageModel,
        config.sherpa.localModelRepoId);

    if (repoId.isEmpty()) {
        LOG_WARN("No matching model found!");
        return false;
    }

    bool sameConfig = m_configCopy.sherpa.threads == config.sherpa.threads
        && m_configCopy.sherpa.useGpu == config.sherpa.useGpu
        && m_configCopy.sherpa.languageModel == config.sherpa.languageModel
        && m_configCopy.sherpa.localModelRepoId == config.sherpa.localModelRepoId;

    if (m_isLoaded && sameConfig) {
        LOG_DEBUG("The model has been loaded.");
        return false;
    }

    loadModelAsync(config, true);
    return true;
}

void SherpaManager::loadModelAsync(const AppConfig& config, bool isReload)
{
    {
        QMutexLocker locker(&m_queueMutex);
        m_queue.enqueue({ TaskType::Load, {}, 0, config, isReload });
        m_queueNotEmpty.wakeOne();
    }
    emit queueSizeChanged(pendingCount());
}

void SherpaManager::loadModel(const AppConfig& config, bool isReload)
{
    QMutexLocker locker(&m_recognizerMutex);
    QString repoId = ModelRegistry::FindByDisplayName(
        config.sherpa.languageModel,
        config.sherpa.localModelRepoId);

    if (repoId.isEmpty()) {
        LOG_WARN("Model not found");
        return;
    }

    if (!SherpaInstaller::isInstalled(repoId)) {
        LOG_WARN("Model not installed");
        return;
    }

    const ModelDescriptor* desc = ModelRegistry::Find(repoId);
    bool punctuatorBound = ModelRegistry::shouldUseNeuralPunct(*desc);
    if (punctuatorBound) {
        QString punctDirName = ModelRegistry::NeuralPunctModel::sharedDir();
        if (!ModelRegistry::NeuralPunctModel::isInstalled()) {

            LOG_WARN(tr("Model or tokens file does not exist! 1%").arg(punctDirName));
        }else {
            if (!m_punctuator->load(punctDirName)) {
                LOG_WARN("Neural punctuation model failed to load; reverting to heuristic punctuation.");
            }
        }
    }else {
        m_punctuator->unload();
    }

    int numThreads = config.sherpa.threads;
    bool useGpu = config.sherpa.useGpu;

    if (!isReload && m_isLoaded && m_currentRepoId == repoId) {
        LOG_DEBUG(QString("Model %1 already loaded, reusing cached recognizer.").arg(repoId));
        return;
    }

    m_isLoaded = false;
    m_offlineRecognizer.reset();
    m_onlineRecognizer.reset();
    m_kind = RecognizerKind::None;
    m_currentRepoId.clear();

	auto result = ModelRegistry::GetConfig(repoId, numThreads, useGpu);
	m_isLoaded = result.isLoaded;
	if (result.cudaFellBack) {
	    emit gpuFallbackToCpu();
	}
	if (!result.isLoaded) {
	    LOG_ERROR("Load failed: model file missing or does not exist");
	    LOG_WARN(tr("Model or tokens file does not exist! 1%").arg(repoId));
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
            LOG_WARN("Variant content does not match RecognizerKind::Offline!");
        }
		break;
	case RecognizerKind::Online:
        if (auto* pOnline = std::get_if<std::unique_ptr<sherpa_onnx::cxx::OnlineRecognizer>>(&result.recognizer)) {
            m_onlineRecognizer = std::move(*pOnline);
            m_kind = RecognizerKind::Online;
        }
        else {
            LOG_WARN("Variant content does not match RecognizerKind::Online!");
        }
		break;
    default:
        break;
    }

    if (m_isLoaded) {
        m_currentRepoId = repoId;
        m_configCopy = config;
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
    // 不再因模型未加载而丢弃：utterance 入队后排在 Load 任务之后，
    // worker 串行执行，加载完成后再转录，避免 Loading 态说话被抛弃。
    {
        QMutexLocker locker(&m_queueMutex);
        m_queue.enqueue({ TaskType::Transcribe, pcmData, sampleRate });
        m_queueNotEmpty.wakeOne(); // 唤醒可能正在休眠等待任务的 worker 线程
    }
    emit queueSizeChanged(pendingCount());
}

void SherpaManager::pauseIdleTimer()
{
    if (m_idleTimer) m_idleTimer->stop();
}

void SherpaManager::resumeIdleTimer()
{
    if (m_idleTimer) m_idleTimer->start();
}

void SherpaManager::workerLoop()
{
    forever{
        PendingTask  task;

        {
            QMutexLocker locker(&m_queueMutex);
            while (m_queue.isEmpty() && !m_stopWorker) {
                m_queueNotEmpty.wait(&m_queueMutex);
            }
            if (m_stopWorker && m_queue.isEmpty()) {
                return;
            }
            task = m_queue.dequeue();
            m_busyFlag = true;
        }
        emit queueSizeChanged(pendingCount());

        switch (task.type) {
        case TaskType::Load:
            loadModel(task.config, task.isReload);
            emit modelLoadFinished(m_isLoaded);      
            break;
        case TaskType::Transcribe: {
            QString text, error;
            bool success = transcribeSync(task.pcmData, task.sampleRate, &text, &error);

            if (success && m_punctuator->isLoaded()) {
                text = m_punctuator->punctuate(text);
            }
            emit utteranceTranscribed(success, text, error);
            break;
        }
        default: break;
        }

        {
            QMutexLocker locker(&m_queueMutex);
            m_busyFlag = false;
        }
    }
}


SherpaInstaller::SherpaInstaller(QNetworkAccessManager* nam, QObject* parent) : QObject(parent)
{
    m_extractQueue = new TaskQueueManager(this);
    connect(this, &SherpaInstaller::installationProgress, this, [this](const QString& msg) {  LOG_DEBUG(msg); });
    connect(this, &SherpaInstaller::installationFinished, this, [this](bool ok, const QString& msg) { ok ? LOG_DEBUG(msg) : LOG_WARN(msg); });
    connect(this, &SherpaInstaller::installFileError, this, [this](const QString&, const QString&, const QString& error) { LOG_WARN(error); });
    connect(this, &SherpaInstaller::installGroupFinished, this, [this](const QString&repoId, bool success, const QString& msg) { 
        if (success) {
            LOG_DEBUG(msg);
            LOG_INFO("Download and configure completed");
        }else {
            LOG_ERROR(msg);
        }
    });

    m_downloadManager = new DownloadManager(nam);

    connect(m_downloadManager, &DownloadManager::groupFileProgress, this, &SherpaInstaller::onGroupFileProgress);
    connect(m_downloadManager, &DownloadManager::groupFileFinished, this, &SherpaInstaller::onGroupFileFinished);
    connect(m_downloadManager, &DownloadManager::groupFileError, this, &SherpaInstaller::onGroupFileError);
    connect(m_downloadManager, &DownloadManager::groupFinished, this, &SherpaInstaller::onGroupFinished);
}

bool SherpaInstaller::isInstalling(const QString& repoId) const
{
    return m_activeManifests.contains(repoId) || !m_downloadManager->tasksInGroup(repoId).isEmpty();
}

bool SherpaInstaller::isInstalled(const QString& repoId)
{
	QString repoName = repoId.split("/").last();
	QString modelPath = ModelConfigFactory::getSherpaModel() + "/" + repoName;
	return QFile::exists(modelPath);
}

void SherpaInstaller::ensurePunctModel(const QString& repoId)
{
    const ModelDescriptor* desc = ModelRegistry::Find(repoId);
    if (!desc) return;
    if (!ModelRegistry::shouldUseNeuralPunct(*desc)) return;
    if (ModelRegistry::NeuralPunctModel::isInstalled()) return;
    if (m_downloadManager->tasksInGroup(ModelRegistry::NeuralPunctModel::repoId).isEmpty()) {
        const QString puncGroup = ModelRegistry::NeuralPunctModel::repoId;
        const QString groupName = "神经标点模型(zh-en)";
        m_downloadManager->addGroupTask(puncGroup,
            groupName,
            QUrl(ModelRegistry::NeuralPunctModel::archiveUrl),
            ModelRegistry::NeuralPunctModel::localPath);
        LOG_INFO("Start downloading punctuation model...");
        emit installGroupStarted(puncGroup, groupName, 1);
    }
}

void SherpaInstaller::installModel(const QString& repoId)
{
    ensurePunctModel(repoId);

    QString repoName = repoId.split("/").last();
    QString modelPath = ModelConfigFactory::getSherpaModel() + "/" + repoName;
    if (QFile::exists(modelPath)) {
        emit installGroupFinished(repoId, true, tr("%1 is exist.").arg(repoId));
        emit loadModel(repoId, true, tr("Download Complete"));
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

void SherpaInstaller::uninstallModel(const QString& repoId)
{
    QString sherpaModelRoot = ModelConfigFactory::getSherpaModel();
    QString repoName = repoId.split("/").last();
    QDir dir(sherpaModelRoot + "/" + repoName);
    if (dir.exists() && dir.removeRecursively()) {
        LOG_INFO("Unload succeeded");
    }
    else {
        LOG_ERROR("Unload failed");
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
    const QString puncGroup = ModelRegistry::NeuralPunctModel::repoId;
    if (groupId == puncGroup) {
        if (!success) {
            emit installGroupFinished(puncGroup, false, tr("Neural punctuation model download failed"));
            return;
        }
        const QString archivePath = ModelRegistry::NeuralPunctModel::localPath;
        const QString targetDir = ModelRegistry::NeuralPunctModel::sharedDir();
        auto* task = new ExtractTask(archivePath, targetDir);
        QObject::connect(task, &ExtractTask::extractStarted, this,
            [this, groupId](int total) { emit extractStarted(groupId, total); });
        QObject::connect(task, &ExtractTask::extractProgress, this,
            [this, groupId](int cur, int tot) { emit extractProgress(groupId, cur, tot); });
        QObject::connect(task, &ExtractTask::extractFinished, this,
            [this, groupId](bool ok) { emit extractFinished(groupId, ok); });
        QObject::connect(task, &ExtractTask::extractFinished, this,
            [this, puncGroup](bool ok) {
                emit installGroupFinished(puncGroup, ok, ok ? tr("Neural punctuation model ready") : tr("Neural punctuation decompression failed"));
            });
        m_extractQueue->addTask(task);
        return;
    }

    if (!success) {
        uninstallModel(groupId);
        emit installGroupFinished(groupId, false, tr("Download Error"));
        return;
    }

    auto it = m_activeManifests.find(groupId);
    if (it == m_activeManifests.end()) {
        emit installGroupFinished(groupId, true, tr("Download Complete"));
        emit loadModel(groupId, true, tr("Download Complete"));
        return;
    }

    const ModelInstallManifest& manifest = it.value();
    emit installationProgress(tr("Decoding in progress %1 ...").arg(manifest.displayName));

    const QString archivePath = manifest.archiveLocalPath;
    const QString targetDir = manifest.archiveTargetDir;
    m_activeManifests.remove(groupId);

    auto* task = new ExtractTask(archivePath, targetDir);
    QObject::connect(task, &ExtractTask::extractStarted, this,
        [this, groupId](int total) { emit extractStarted(groupId, total); });
    QObject::connect(task, &ExtractTask::extractProgress, this,
        [this, groupId](int cur, int tot) { emit extractProgress(groupId, cur, tot); });
    QObject::connect(task, &ExtractTask::extractFinished, this,
        [this, groupId](bool ok) { emit extractFinished(groupId, ok); });
    QObject::connect(task, &BaseTask::taskFinished, this,
        [this, groupId](TaskResult result, const QString&) {
            onSherpaExtractFinished(groupId, result == TaskResult::Success);
        });

    m_extractQueue->addTask(task);
}

void SherpaInstaller::onSherpaExtractFinished(const QString& groupId, bool ok)
{
    if (!ok) {
        emit installGroupFinished(groupId, false, tr("Extract Error"));
        return;
    }
    emit installGroupFinished(groupId, true, tr("Download Complete"));
    emit loadModel(groupId, true, tr("Download Complete"));
}

SherpaDependencyManager::SherpaDependencyManager(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent)
{}

void SherpaDependencyManager::checkAndRepairDependencies(const QString& targetDir, bool useCuda, Downloader* downloader)
{
    m_targetDir = targetDir;
    m_downloader = downloader;
    m_isCuda = useCuda;
    m_currentIndex = 0;
    m_queue.clear();

    connect(m_downloader, &Downloader::finished, this, &SherpaDependencyManager::onDownloadFinished);
    connect(m_downloader, &Downloader::error, this, &SherpaDependencyManager::onDownloadError);
    startNext();
}

void SherpaDependencyManager::startNext()
{
    if (m_currentIndex >= m_queue.size()) {
        emit finished(true, "All dependencies downloaded successfully.");
        return;
    }

    const auto& dep = m_queue[m_currentIndex];
    QString savePath = QDir(m_targetDir).filePath(dep.archiveName);

    emit progress(QString("Downloading %1...").arg(dep.archiveName), 0);
    m_downloader->start(dep.url, savePath);
}


void SherpaDependencyManager::onDownloadFinished(const QString& path)
{
    emit progress("Extracting files...", 90);

    ExtractOptions opts;
    opts.destinationDir = m_targetDir;
    opts.filter = [](const QFileInfo& fi) {
        return fi.fileName().startsWith("onnx", Qt::CaseInsensitive);
        };
    if (ExtractTool::extractAndDeploy(path, opts)) {
        m_currentIndex++;
        startNext();
    }else {
        emit finished(false, "Extraction failed. Please check disk space or permissions.");
    }
}

void SherpaDependencyManager::onDownloadError(const QString& err)
{
    emit finished(false, "Download failed: " + err);
}
