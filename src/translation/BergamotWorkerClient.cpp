#include "BergamotWorkerClient.h"
#include "../utils/Logger.h"
#include "../../tools/translate_worker/WorkerProtocol.h"

#include <QFileInfo>
#include <QProcess>

namespace wp = worker_protocol;

BergamotWorkerClient::BergamotWorkerClient(RouteProvider provider, const QString& workerPath, QObject* parent)
    : ITranslationBackend(parent), m_provider(std::move(provider)), m_workerPath(workerPath)
{
}

BergamotWorkerClient::~BergamotWorkerClient() {
    stopWorker();
}

bool BergamotWorkerClient::isUsable() const {
    return QFileInfo::exists(m_workerPath);
}

bool BergamotWorkerClient::isAvailable(const QString& from, const QString& to) const {
    if (!isUsable()) return false;

    const TranslationRoute route = m_provider.plan(from, to);
    if (route.kind != RouteKind::Direct && route.kind != RouteKind::Pivot) return false;
    for (const TranslationHop& hop : route.hops) {
        if (!m_provider.isInstalled(hop.from, hop.to)) return false;
    }
    return true;
}

void BergamotWorkerClient::translate(qint64 id, const QString& text, const QString& from, const QString& to) {
    const TranslationRoute route = m_provider.plan(from, to);
    if (route.kind != RouteKind::Direct && route.kind != RouteKind::Pivot) {
        emit translated(id, false, {}, QStringLiteral("不支持该语言方向"));
        return;
    }

    if (!m_started) {
        m_started = true;
        startWorker();
    }

    // 先加载路线上还没加载的方向，worker 串行处理，所以加载一定先于翻译请求完成
    for (const TranslationHop& hop : route.hops) {
        const QString key = hop.from + QLatin1Char('>') + hop.to;
        if (m_loaded.contains(key)) continue;

        const QString config = m_provider.configPath(hop.from, hop.to);
        if (config.isEmpty()) {
            emit translated(id, false, {}, QStringLiteral("翻译模型未安装"));
            return;
        }
        m_loaded.insert(key);
        enqueue(QByteArray::fromStdString(wp::formatLoad(hop.from.toStdString(), hop.to.toStdString(),
                                                         config.toStdString())));
    }

    m_pending.insert(id);
    enqueue(QByteArray::fromStdString(wp::formatTranslate(id, from.toStdString(), to.toStdString(),
                                                          text.toStdString())));
}

void BergamotWorkerClient::enqueue(const QByteArray& line) {
    if (m_ready) sendLine(line);
    else m_queue.append(line);
}

void BergamotWorkerClient::flushQueue() {
    const QList<QByteArray> queued = std::move(m_queue);
    m_queue.clear();
    for (const QByteArray& line : queued) sendLine(line);
}

void BergamotWorkerClient::handleLine(const QString& line) {
    const wp::Response r = wp::parseResponse(line.toStdString());

    switch (r.kind) {
    case wp::Response::Kind::Ready:
        m_ready = true;
        flushQueue();
        break;

    case wp::Response::Kind::Result:
        if (m_pending.remove(r.id)) {
            emit translated(r.id, true, QString::fromStdString(r.text), {});
        }
        break;

    case wp::Response::Kind::Error: {
        const QString code = QString::fromStdString(r.code);
        const QString message = QString::fromStdString(r.text);
        LOG_WARN(QString("Translation worker error %1: %2").arg(code, message));

        if (r.id != 0) {
            // worker 说没加载这个方向：多半是重启过，已加载的记录失效了，清掉让下次重新加载
            if (code == QLatin1String(wp::kNoModel)) m_loaded.clear();
            if (m_pending.remove(r.id)) emit translated(r.id, false, {}, friendlyError(code, message));
        }
        else {
            // 加载失败不属于某个请求：记录失效，后面的请求会以 NO_MODEL 失败
            m_loaded.clear();
        }
        break;
    }

    case wp::Response::Kind::Loaded:
        LOG_DEBUG(QString("Translation model loaded: %1 -> %2 (%3 ms)").arg(QString::fromStdString(r.from),
                  QString::fromStdString(r.to)).arg(r.ms));
        break;

    case wp::Response::Kind::Invalid:
        LOG_WARN(QString("Unexpected line from translation worker: %1").arg(line.left(120)));
        break;
    }
}

QString BergamotWorkerClient::friendlyError(const QString& code, const QString& message) {
    if (code == QLatin1String(wp::kNoModel)) return QStringLiteral("翻译模型未加载");
    return QStringLiteral("翻译引擎出错：%1").arg(message);
}

void BergamotWorkerClient::handleWorkerGone(const QString& reason) {
    LOG_WARN(QString("Translation worker stopped: %1").arg(reason));
    m_started = false;
    m_ready = false;
    m_queue.clear();
    m_loaded.clear();
    m_stdoutBuffer.clear();
    failAll(QStringLiteral("翻译引擎已退出"));
}

void BergamotWorkerClient::failAll(const QString& cause) {
    const QSet<qint64> pending = std::move(m_pending);
    m_pending.clear();
    for (qint64 id : pending) emit translated(id, false, {}, cause);
}

// ---- 真实进程；测试里这三个函数被替换

void BergamotWorkerClient::startWorker() {
    if (m_process) {   // 上一个进程已退出，丢掉旧对象
        m_process->disconnect(this);
        m_process->deleteLater();
    }
    m_process = new QProcess(this);
    m_process->setProgram(m_workerPath);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        m_stdoutBuffer += m_process->readAllStandardOutput();
        int newline;
        while ((newline = m_stdoutBuffer.indexOf('\n')) >= 0) {
            QByteArray raw = m_stdoutBuffer.left(newline);
            m_stdoutBuffer.remove(0, newline + 1);
            if (raw.endsWith('\r')) raw.chop(1);
            if (!raw.isEmpty()) handleLine(QString::fromUtf8(raw));
        }
    });
    // stderr 必须读空，否则管道写满会阻塞 worker；内容记入日志，便于排查启动和加载失败的原因
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        const QString text = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        if (!text.isEmpty()) LOG_WARN(QString("translate_worker: %1").arg(text));
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) handleWorkerGone(QStringLiteral("failed to start"));
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        handleWorkerGone(QStringLiteral("exit code %1").arg(code));
    });

    m_process->start();
}

void BergamotWorkerClient::sendLine(const QByteArray& line) {
    if (!m_process) return;
    m_process->write(line + '\n');
}

void BergamotWorkerClient::stopWorker() {
    if (!m_process) return;

    m_process->disconnect(this);
    m_process->write(QByteArray::fromStdString(wp::formatQuit()) + '\n');
    m_process->closeWriteChannel();
    if (!m_process->waitForFinished(2000)) m_process->kill();
    m_process->deleteLater();
    m_process = nullptr;
}
