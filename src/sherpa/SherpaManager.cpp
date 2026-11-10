#include "SherpaManager.h"
#include "InstallMarker.h"
#include <QFileInfo>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <QtConcurrent> 
#include "utils/Logger.h"
#include "utils/Diagnostics.h"
#include "utils/AppPaths.h"
#include <QElapsedTimer>
#include "utils/ExtractTool.h"
#include "config/ConfigManager.h"



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
        emit idleUnloaded();
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
        for (const PendingTask& t : std::as_const(m_queue)) {
            if (t.type == TaskType::Load) --m_loadsInFlight;
        }
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

    auto sameModel = [&config](const AppConfig& other) {
        return other.sherpa.threads == config.sherpa.threads
            && other.sherpa.useGpu == config.sherpa.useGpu
            && other.sherpa.languageModel == config.sherpa.languageModel
            && other.sherpa.localModelRepoId == config.sherpa.localModelRepoId;
    };

    if (m_isLoaded && sameModel(m_configCopy)) {
        LOG_DEBUG("The model has been loaded.");
        return false;
    }

    // 启动预加载还没跑完时就开始录音：再排一个加载会把已加载好的识别器清掉重来，
    // 让 Loading 态的判断和第一句的转录都撞上重载窗口，直接等在途的那次加载完成即可
    {
        QMutexLocker locker(&m_queueMutex);
        if (m_loadsInFlight > 0 && sameModel(m_lastLoadRequest)) {
            LOG_DEBUG("Model load already in flight, waiting for it.");
            return true;
        }
    }

    loadModelAsync(config, true);
    return true;
}

void SherpaManager::loadModelAsync(const AppConfig& config, bool isReload)
{
    {
        QMutexLocker locker(&m_queueMutex);
        ++m_loadsInFlight;
        m_lastLoadRequest = config;
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
        LOG_WARN(QString("Model not found (languageModel=%1, localModelRepoId=%2)")
                     .arg(config.sherpa.languageModel, config.sherpa.localModelRepoId));
        return;
    }

    // 选中的是 CPU 模型；GPU 模式且 GPU 版已下载时，实际加载它的 GPU 版（没下完就继续用 CPU 版）
    const QString selectedRepoId = repoId;
    repoId = ModelRegistry::ResolveForMode(selectedRepoId, config.sherpa.useGpu);
    if (repoId != selectedRepoId) {
        LOG_INFO(QString("Using GPU variant %1 for selected model %2").arg(repoId, selectedRepoId));
    }

    const QString modelDir = ModelConfigFactory::getSherpaModel() + "/" + repoId.split("/").last();
    LOG_DEBUG(QString("Model load | repo=%1 dir=%2 threads=%3 gpu=%4 reload=%5")
                  .arg(repoId, modelDir).arg(config.sherpa.threads).arg(config.sherpa.useGpu).arg(isReload));

    if (!SherpaInstaller::isInstalled(repoId)) {
        LOG_WARN(QString("Model not installed: %1").arg(modelDir));
        return;
    }
    Diagnostics::logDirectoryListing("Model load | files", modelDir);

    const ModelDescriptor* desc = ModelRegistry::Find(repoId);
    bool punctuatorBound = ModelRegistry::shouldUseNeuralPunct(*desc, config.sherpa.languageModel);
    if (punctuatorBound) {
        QString punctDirName = ModelRegistry::NeuralPunctModel::sharedDir();
        if (!ModelRegistry::NeuralPunctModel::isInstalled()) {

            LOG_WARN(tr("Model or tokens file does not exist! %1").arg(punctDirName));
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

    auto releaseCurrent = [this]() {
        m_isLoaded = false;
        m_offlineRecognizer.reset();
        m_onlineRecognizer.reset();
        m_kind = RecognizerKind::None;
        m_currentRepoId.clear();
    };
    // GPU 变体用 CUDA 且不回退 CPU：创建失败时要保住已加载的模型，所以成功后才释放旧的
    // （代价是切换瞬间两个模型同时在内存里）
    const bool keepOldOnFail = ModelRegistry::ShouldUseCuda(*desc, useGpu);
    if (!keepOldOnFail) releaseCurrent();

	QElapsedTimer loadTimer;
	loadTimer.start();
	auto result = ModelRegistry::GetConfig(repoId, numThreads, useGpu);
	if (!result.isLoaded) {
	    if (result.cudaFailed) {
	        emit loadFailed(QStringLiteral("GPU 加速初始化失败"),
	                        QStringLiteral("该模型需要显卡运行，但 CUDA 初始化失败，请检查显卡驱动与 CUDA 组件，或切换到 CPU 模式"));
	    }
	    if (!keepOldOnFail) m_isLoaded = false;
	    LOG_ERROR(QString("Load failed: recognizer not created for %1 after %2 ms (dir=%3)，"
	                      "具体原因见上方 Failed to create recognizer 记录与 voice_ime.stderr.log")
	                  .arg(repoId).arg(loadTimer.elapsed()).arg(modelDir));
	    return;
	}
	if (keepOldOnFail) releaseCurrent();
	m_isLoaded = true;

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
        LOG_DEBUG(QString("Model load | ok: %1 kind=%2 in %3 ms")
                      .arg(repoId).arg(static_cast<int>(m_kind)).arg(loadTimer.elapsed()));
    } else {
        LOG_ERROR(QString("Model load | recognizer variant unusable for %1 (kind=%2)")
                      .arg(repoId).arg(static_cast<int>(result.kind)));
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

        if (result.text.empty()) {
            LOG_WARN(QString("Offline recognizer returned empty text (repo=%1, samples=%2)")
                .arg(m_currentRepoId).arg(samples.size()));
        }

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
    m_idlePaused = true;
    if (m_idleTimer) m_idleTimer->stop();
}

void SherpaManager::resumeIdleTimer()
{
    m_idlePaused = false;
    if (m_idleTimer) m_idleTimer->start();
}

// 在 worker 线程调用：定时器属于主线程，且录音 / 文件转录期间不能武装，否则长任务会被空闲卸载打断。
// 暂停标志在主线程上再检查一次，避免排队期间刚好进入暂停
void SherpaManager::armIdleTimer()
{
    QMetaObject::invokeMethod(this, [this]() {
        if (!m_idlePaused && m_isLoaded) m_idleTimer->start();
    }, Qt::QueuedConnection);
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
            {
                QMutexLocker locker(&m_queueMutex);
                --m_loadsInFlight;
            }
            if (m_isLoaded) armIdleTimer();
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

namespace {
QString modelDirFor(const QString& repoId)
{
    return ModelConfigFactory::getSherpaModel() + "/" + repoId.split("/").last();
}
}

bool SherpaInstaller::isInstalled(const QString& repoId)
{
    // 目录存在且没有"安装中"标记才算装好；下到一半被杀留下的目录不算（升级前装好的旧模型没有标记，仍算已安装）
    return InstallMarker::isInstalled(modelDirFor(repoId));
}

void SherpaInstaller::ensurePunctModel(const QString& repoId)
{
    const ModelDescriptor* desc = ModelRegistry::Find(repoId);
    if (!desc) return;
    if (!ModelRegistry::shouldUseNeuralPunct(*desc, ConfigManager::instance().config().sherpa.languageModel)) return;
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

void SherpaInstaller::ensureStreamingModel(const QString& language)
{
    const QString repoId = ModelRegistry::StreamingModelFor(language);
    if (repoId.isEmpty()) return;

    if (isInstalled(repoId)) {
        emit streamingModelReady(repoId);
        return;
    }
    if (isInstalling(repoId)) return;

    // 上次没下完的残留先清掉，重新下
    if (InstallMarker::isIncomplete(modelDirFor(repoId))) QDir(modelDirFor(repoId)).removeRecursively();

    ModelInstallManifest manifest = ModelRegistry::BuildManifest(repoId);
    if (manifest.files.empty()) {
        LOG_WARN(QString("Live caption model has no files to download: %1").arg(repoId));
        return;
    }
    LOG_INFO(QString("Downloading live caption model: %1").arg(repoId));
    if (!InstallMarker::markInstalling(modelDirFor(repoId))) LOG_WARN(QString("Cannot create model directory: %1").arg(repoId));
    m_companionGroups.insert(repoId);
    emit installGroupStarted(repoId, manifest.displayName, static_cast<int>(manifest.files.size()));
    for (const auto& f : manifest.files) {
        m_downloadManager->addGroupTask(repoId, manifest.displayName, f.sourceUrl, f.localPath);
    }
}

void SherpaInstaller::installModel(const QString& repoId)
{
    ensurePunctModel(repoId);
    {
        const AppConfig& cfg = ConfigManager::instance().config();
        if (cfg.liveCaptionEnabled.load()) ensureStreamingModel(cfg.sherpa.languageModel);
    }

    // 正在下载：别重复开一轮，也别把半截目录当成已存在
    if (isInstalling(repoId)) return;

    const QString modelPath = modelDirFor(repoId);
    if (InstallMarker::isInstalled(modelPath)) {
        emit installGroupFinished(repoId, true, tr("%1 is exist.").arg(repoId));
        emit loadModel(repoId, true, tr("Download Complete"));
        return;
    }
    // 上次没下完（带"安装中"标记）：清掉残留重新下
    if (InstallMarker::isIncomplete(modelPath)) {
        LOG_WARN(QString("Removing incomplete model directory: %1").arg(modelPath));
        QDir(modelPath).removeRecursively();
    }

    ModelInstallManifest manifest = ModelRegistry::BuildManifest(repoId);
    if (!InstallMarker::markInstalling(modelPath)) LOG_WARN(QString("Cannot create model directory: %1").arg(modelPath));

    // 先发"开始"再提交下载：文件在启动时就同步失败（目标路径建不出来）时，"失败"会在 addGroupTask 里同步发出，
    // 如果"开始"排在后面，行内状态会先变失败、再被"开始"盖回下载中，永远停在那里
    if (!manifest.archiveUrl.isEmpty()) {
        m_activeManifests[repoId] = manifest;
        emit installGroupStarted(repoId, manifest.displayName, 1);
        m_downloadManager->addGroupTask(repoId, manifest.displayName, QUrl(manifest.archiveUrl), manifest.archiveLocalPath);
        return;
    }

    emit installGroupStarted(repoId, manifest.displayName, static_cast<int>(manifest.files.size()));
    for (const auto& f : manifest.files) {
        m_downloadManager->addGroupTask(repoId, manifest.displayName, f.sourceUrl, f.localPath);
    }
}

void SherpaInstaller::cancelInstall(const QString& repoId)
{
    if (m_downloadManager->tasksInGroup(repoId).isEmpty()) return;   // 没在下载（装好了，或在解压）
    emit installCancelled(repoId);
    // 下载层会随后报告"组失败"，onGroupFinished 的失败分支负责清理目录和记录
    m_downloadManager->cancelGroup(repoId);
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


void SherpaInstaller::ensureVadModel()
{
    const QString vadGroup = ModelRegistry::VadModel::repoId;
    const QString path = ModelRegistry::VadModel::localPath;
    if (QFile::exists(path) || QFile::exists(AppPaths::vadModelFile())) {
        emit vadModelReady(true, tr("VAD model ready"));
        return;
    }

    if (isInstalling(vadGroup)) return;

    QDir().mkpath(QFileInfo(path).absolutePath());
    LOG_INFO("VAD model (silero_vad.onnx) missing, auto-downloading...");

    const QString displayName = tr("VAD Voice Activity Model");
    m_downloadManager->addGroupTask(vadGroup, displayName, QUrl(ModelRegistry::VadModel::archiveUrl), path);
    emit installGroupStarted(vadGroup, displayName, 1);
}

void SherpaInstaller::onGroupFinished(const QString& groupId, bool success)
{
    const QString vadGroup = ModelRegistry::VadModel::repoId;
    if (groupId == vadGroup) {
        const QString msg = success ? tr("VAD model ready") : tr("VAD model download failed");
        if (!success) {
            QFile::remove(ModelRegistry::VadModel::localPath);
        }
        emit installGroupFinished(groupId, success, msg);
        emit vadModelReady(success, msg);
        return;
    }

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

    if (m_companionGroups.contains(groupId)) {
        m_companionGroups.remove(groupId);
        if (!success) {
            uninstallModel(groupId);
            emit installGroupFinished(groupId, false, tr("Download Error"));
            return;
        }
        InstallMarker::markComplete(modelDirFor(groupId));
        emit installGroupFinished(groupId, true, tr("Download Complete"));
        emit streamingModelReady(groupId);
        return;
    }

    if (!success) {
        // 压缩包类模型失败时 m_activeManifests 里还留着记录，不清掉 isInstalling 恒为真，重试会被挡住
        m_activeManifests.remove(groupId);
        uninstallModel(groupId);
        emit installGroupFinished(groupId, false, tr("Download Error"));
        return;
    }

    auto it = m_activeManifests.find(groupId);
    if (it == m_activeManifests.end()) {
        InstallMarker::markComplete(modelDirFor(groupId));
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
    InstallMarker::markComplete(modelDirFor(groupId));
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
