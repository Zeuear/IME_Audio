#include "SingleInstanceGuard.h"

#include <QCryptographicHash>
#include <QDir>
#include <QLocalSocket>
#include <QLockFile>

#include "Logger.h"

SingleInstanceGuard::SingleInstanceGuard(const QString& dataDir, QObject* parent)
    : QObject(parent), m_dataDir(dataDir) {}

SingleInstanceGuard::~SingleInstanceGuard() = default;

QString SingleInstanceGuard::serverNameForDataDir(const QString& dataDir)
{
    // 截断到 16 位，Unix 域套接字的路径长度上限约 104 字节。
    // 必须用确定性哈希：qHash 在 Qt 6 里每进程重新播种，两个进程会算出不同的
    // 名字，通知通道将永远接不上。
    const QByteArray digest =
        QCryptographicHash::hash(dataDir.toUtf8(), QCryptographicHash::Md5).toHex().left(16);
    return "ImeAudio-" + QString::fromLatin1(digest);
}

bool SingleInstanceGuard::acquireOrNotifyExisting()
{
    if (m_lock) return true;

    auto lock = std::make_unique<QLockFile>(QDir(m_dataDir).absoluteFilePath("ImeAudio.lock"));
    lock->setStaleLockTime(0);   // 只按 PID 存活判定陈旧，不按时间

    if (!lock->tryLock(100)) {
        LOG_DEBUG("SingleInstance: another instance holds the lock");
        QLocalSocket socket;
        socket.connectToServer(serverNameForDataDir(m_dataDir));
        if (socket.waitForConnected(1000)) {
            socket.abort();
        }
        return false;
    }
    m_lock = std::move(lock);

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    const QString name = serverNameForDataDir(m_dataDir);
    // 上一个实例崩溃后 Unix 会残留套接字文件，不清掉监听会一直失败
    QLocalServer::removeServer(name);
    if (!m_server->listen(name)) {
        // 通知通道起不来不影响唯一性判定，降级即可
        LOG_DEBUG(QString("SingleInstance: notify channel unavailable: %1")
                      .arg(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        return true;
    }

    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* client = m_server->nextPendingConnection()) {
            client->deleteLater();
            emit anotherInstanceStarted();
        }
    });
    return true;
}
