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

DocsBridge::DocsBridge(QObject* parent) : QObject(parent) {}

DocsBridge::~DocsBridge() { blockSignals(true); setEnabled(false); }

void DocsBridge::setEnabled(bool on)
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
        return;
    }
    m_server = new QWebSocketServer(QStringLiteral("ImeAudio"), QWebSocketServer::NonSecureMode, this);
    m_server->setSupportedSubprotocols({});
    connect(m_server, &QWebSocketServer::newConnection, this, &DocsBridge::onNewConnection);
    if (!m_server->listen(QHostAddress::LocalHost, DocsBridgeProtocol::kPort)) {
        LOG_WARN(QString("DocsBridge listen failed: %1").arg(m_server->errorString()));
        m_server->deleteLater();
        m_server = nullptr;
        return;
    }
    LOG_INFO(QString("DocsBridge listening on 127.0.0.1:%1").arg(DocsBridgeProtocol::kPort));
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
    m_client = nullptr;
    m_authed = false;
    if (m_bound) {
        LOG_WARN("DocsBridge connection lost while a tab was bound, holding output");
        setLink(false, true, m_title);
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
        QSettings s(ConfigManager::configFilePath(), QSettings::IniFormat);
        if (DocsBridgeProtocol::tokenMatches(o.value("token").toString(), s.value(kTokenHashKey).toByteArray())) {
            m_authed = true;
            LOG_INFO("DocsBridge pairing ok");
            // 让扩展知道配对通过，它才能在角标和弹窗里显示"已连接"
            m_client->sendTextMessage(QString::fromUtf8(DocsBridgeProtocol::typeMessage("ready")));
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
    if (type == "bound") setLink(true, false, o.value("title").toString());
    else if (type == "unbound") setLink(false, false, {});
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
    setLink(false, false, {});
}

bool DocsBridge::sendText(const QString& text)
{
    if (!m_authed || !m_bound || !m_client) {
        LOG_DEBUG(QString("DocsBridge sendText skipped (client=%1 authed=%2 bound=%3)")
                      .arg(m_client != nullptr).arg(m_authed).arg(m_bound));
        return false;
    }
    m_client->sendTextMessage(QString::fromUtf8(DocsBridgeProtocol::textMessage(text)));
    LOG_INFO(QString("DocsBridge text sent to extension (%1 chars)").arg(text.size()));
    return true;
}
