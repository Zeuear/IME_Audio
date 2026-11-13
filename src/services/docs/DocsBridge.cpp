#include "DocsBridge.h"
#include "utils/Logger.h"
#include <QWebSocketServer>
#include <QWebSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSettings>
#include "config/ConfigManager.h"

namespace {
const QString kTokenHashKey = QStringLiteral("docs/pairingTokenHash");
const QString kLegacyTokenKey = QStringLiteral("docs/pairingToken");
}

DocsBridge::DocsBridge(QObject* parent)
    : QObject(parent), m_session(newToken().left(12))
{
    m_tokenHash = []() {
        QSettings s(ConfigManager::configFilePath(), QSettings::IniFormat);
        return s.value(kTokenHashKey).toByteArray();
    };
}

DocsBridge::~DocsBridge() { blockSignals(true); setEnabled(false); }

quint16 DocsBridge::serverPort() const
{
    return m_server ? m_server->serverPort() : 0;
}

void DocsBridge::setEnabled(bool on, quint16 port)
{
    if (on == (m_server != nullptr)) return;
    if (!on) {
        LOG_INFO("DocsBridge stopped");
        if (m_client) { m_client->disconnect(this); m_client->close(); m_client->deleteLater(); }
        m_client = nullptr;
        m_authed = false;
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
        setLink(false, false, {});
        clearOutbox("bridge stopped");
        return;
    }
    m_server = new QWebSocketServer(QStringLiteral("ImeAudio"), QWebSocketServer::NonSecureMode, this);
    m_server->setSupportedSubprotocols({});
    connect(m_server, &QWebSocketServer::newConnection, this, &DocsBridge::onNewConnection);
    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        LOG_WARN(QString("DocsBridge listen failed: %1").arg(m_server->errorString()));
        m_server->deleteLater();
        m_server = nullptr;
        return;
    }
    LOG_INFO(QString("DocsBridge listening on 127.0.0.1:%1").arg(m_server->serverPort()));
}

void DocsBridge::onNewConnection()
{
    QWebSocket* s = m_server->nextPendingConnection();
    if (!s) return;
    if (!DocsBridgeProtocol::isAllowedOrigin(s->origin())) {
        LOG_WARN(QString("DocsBridge rejected origin: \"%1\" (expected chrome-extension id from manifest key)")
                     .arg(s->origin().left(80)));
        s->close(QWebSocketProtocol::CloseCodePolicyViolated);
        s->deleteLater();
        return;
    }
    if (m_client) {
        m_client->disconnect(this);
        m_client->close();
        m_client->deleteLater();
        dropClient();
    }
    LOG_INFO("DocsBridge extension connected, waiting for hello");
    m_client = s;
    connect(s, &QWebSocket::textMessageReceived, this, &DocsBridge::onMessage);
    connect(s, &QWebSocket::disconnected, this, [this, s]() {
        if (m_client != s) return;
        LOG_INFO(QString("DocsBridge extension disconnected (authed=%1, code=%2)").arg(m_authed).arg(s->closeCode()));
        s->deleteLater();
        dropClient();
    });
}

void DocsBridge::dropClient()
{
    const bool wasAuthed = m_authed;
    m_client = nullptr;
    m_authed = false;
    if (m_bound) {
        LOG_WARN("DocsBridge connection lost while a tab was bound, holding output");
        setLink(false, true, m_title);
    } else if (wasAuthed) {
        emit linkChanged();
    }
}

void DocsBridge::onMessage(const QString& msg)
{
    QJsonParseError err;
    const QJsonObject o = QJsonDocument::fromJson(msg.toUtf8(), &err).object();
    if (err.error != QJsonParseError::NoError) {
        LOG_WARN(QString("DocsBridge ignored non-JSON message (%1 chars): %2").arg(msg.size()).arg(err.errorString()));
        return;
    }
    const QString type = o.value("type").toString();
    if (type == "hello") {
        if (DocsBridgeProtocol::tokenMatches(o.value("token").toString(), m_tokenHash())) {
            m_authed = true;
            LOG_INFO("DocsBridge pairing ok");
            // 让扩展知道配对通过，它才能在角标和弹窗里显示"已连接"
            m_client->sendTextMessage(QString::fromUtf8(DocsBridgeProtocol::typeMessage("ready")));
            emit linkChanged();
            return;
        }
        LOG_WARN(QString("DocsBridge rejected wrong pairing token (got %1 chars)").arg(o.value("token").toString().size()));
        m_client->sendTextMessage(QString::fromUtf8(DocsBridgeProtocol::typeMessage("denied")));
        m_client->close(QWebSocketProtocol::CloseCodePolicyViolated);
        return;
    }
    if (!m_authed) {
        LOG_WARN(QString("DocsBridge dropped \"%1\" before hello/pairing").arg(type.left(20)));
        return;
    }
    if (type == "ack") {
        const quint64 id = quint64(o.value("id").toDouble());
        while (!m_outbox.isEmpty() && m_outbox.first().id <= id) m_outbox.removeFirst();
    }
    else if (type == "bound") {
        const bool wasBound = m_bound;
        setLink(true, false, o.value("title").toString());
        // 刚绑上（含断线重连后恢复绑定）才补发；同一绑定只更新标题时不重发
        if (!wasBound) flushOutbox();
    }
    else if (type == "unbound") {
        // 用户主动解绑：队列里的句子不再写进文档，听写记录里仍有
        clearOutbox("tab unbound");
        setLink(false, false, {});
    }
    else if (type == "insertFailed") {
        const QString reason = o.value("reason").toString().left(200);
        LOG_WARN(QString("DocsBridge extension failed to insert id=%1: %2").arg(o.value("id").toDouble()).arg(reason));
        setLink(false, true, m_title);
        emit insertFailed(reason);
    }
    else if (type == "bindFailed") {
        const QString reason = o.value("reason").toString().left(200);
        LOG_WARN(QString("DocsBridge extension failed to bind: %1").arg(reason));
        emit bindFailed(reason);
    }
    else if (type == "ping") return;
    else LOG_DEBUG(QString("DocsBridge ignored unknown message type \"%1\"").arg(type.left(20)));
}

QString DocsBridge::newToken()
{
    QByteArray raw(16, 0);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(raw.data()), raw.size() / 4);
    return QString::fromLatin1(raw.toHex());
}

void DocsBridge::saveToken(const QString& token)
{
    QSettings s(ConfigManager::configFilePath(), QSettings::IniFormat);
    s.setValue(kTokenHashKey, DocsBridgeProtocol::tokenHash(token));
    s.remove(kLegacyTokenKey);
}

bool DocsBridge::hasToken()
{
    QSettings s(ConfigManager::configFilePath(), QSettings::IniFormat);
    return !s.value(kTokenHashKey).toByteArray().isEmpty();
}

bool DocsBridge::isCurrentToken(const QString& token)
{
    QSettings s(ConfigManager::configFilePath(), QSettings::IniFormat);
    return DocsBridgeProtocol::tokenMatches(token, s.value(kTokenHashKey).toByteArray());
}

void DocsBridge::setLink(bool bound, bool lost, const QString& title)
{
    if (m_bound == bound && m_lost == lost && m_title == title) return;
    // 文档标题属于用户内容，只记状态
    LOG_INFO(QString("DocsBridge link: bound=%1 lost=%2").arg(bound).arg(lost));
    m_bound = bound;
    m_lost = lost;
    m_title = title;
    emit linkChanged();
}

void DocsBridge::release()
{
    if (m_client && m_authed && m_bound)
        m_client->sendTextMessage(QString::fromUtf8(DocsBridgeProtocol::typeMessage("release")));
    clearOutbox("released by user");
    setLink(false, false, {});
}

void DocsBridge::clearOutbox(const char* why)
{
    if (m_outbox.isEmpty()) return;
    LOG_INFO(QString("DocsBridge dropped %1 pending sentence(s): %2").arg(m_outbox.size()).arg(why));
    m_outbox.clear();
}

void DocsBridge::flushOutbox()
{
    if (!canSend() || m_outbox.isEmpty()) return;
    LOG_INFO(QString("DocsBridge resending %1 pending sentence(s)").arg(m_outbox.size()));
    m_client->sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{
        {"type", "resume"}, {"session", m_session}, {"from", double(m_outbox.first().id)}}).toJson(QJsonDocument::Compact)));
    for (const Pending& p : m_outbox)
        m_client->sendTextMessage(QString::fromUtf8(DocsBridgeProtocol::textMessage(m_session, p.id, p.text)));
}

bool DocsBridge::sendCommand(const QString& type)
{
    if (!m_client || !m_authed) return false;
    LOG_INFO(QString("DocsBridge command sent: %1").arg(type));
    m_client->sendTextMessage(QString::fromUtf8(DocsBridgeProtocol::typeMessage(type)));
    return true;
}

bool DocsBridge::requestNewDoc() { return sendCommand(QStringLiteral("newDoc")); }

bool DocsBridge::requestBindActive() { return sendCommand(QStringLiteral("bindActive")); }

bool DocsBridge::sendText(const QString& text)
{
    if (!holdsOutput()) return false;
    const Pending p{m_nextId++, text};
    m_outbox.append(p);
    if (m_outbox.size() > m_maxPending) {
        LOG_WARN("DocsBridge outbox full, dropping the oldest pending sentence");
        m_outbox.removeFirst();
        ++m_dropped;
    }
    if (!canSend()) {
        LOG_INFO(QString("DocsBridge held sentence id=%1 until the tab is bound again (%2 pending)")
                     .arg(p.id).arg(m_outbox.size()));
        return true;
    }
    m_client->sendTextMessage(QString::fromUtf8(DocsBridgeProtocol::textMessage(m_session, p.id, text)));
    // 只记长度，听写内容不进日志
    LOG_DEBUG(QString("DocsBridge sent id=%1 (%2 chars)").arg(p.id).arg(text.size()));
    return true;
}
