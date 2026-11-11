#include "DocsBridge.h"
#include "utils/Logger.h"
#include <QWebSocketServer>
#include <QWebSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSettings>
#include "config/ConfigManager.h"

DocsBridge::DocsBridge(QObject* parent) : QObject(parent) {}

DocsBridge::~DocsBridge() { setEnabled(false); }

void DocsBridge::setEnabled(bool on)
{
    if (on == (m_server != nullptr)) return;
    if (!on) {
        if (m_client) m_client->close();
        m_authed = false;
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
        m_client = nullptr;
        setBound(false, {});
        return;
    }
    m_server = new QWebSocketServer(QStringLiteral("ImeAudio"), QWebSocketServer::NonSecureMode, this);
    m_server->setSupportedSubprotocols({});
    connect(m_server, &QWebSocketServer::newConnection, this, &DocsBridge::onNewConnection);
    if (!m_server->listen(QHostAddress::LocalHost, DocsBridgeProtocol::kPort)) {
        LOG_WARN(QString("DocsBridge listen failed: %1").arg(m_server->errorString()));
        m_server->deleteLater();
        m_server = nullptr;
    }
}

void DocsBridge::onNewConnection()
{
    QWebSocket* s = m_server->nextPendingConnection();
    if (!s) return;
    if (!DocsBridgeProtocol::isAllowedOrigin(s->origin())) {
        LOG_WARN(QString("DocsBridge rejected origin: %1").arg(s->origin()));
        s->close(QWebSocketProtocol::CloseCodePolicyViolated);
        s->deleteLater();
        return;
    }
    if (m_client) { m_client->disconnect(this); m_client->close(); m_client->deleteLater(); }
    m_client = s;
    m_authed = false;
    setBound(false, {});
    connect(s, &QWebSocket::textMessageReceived, this, &DocsBridge::onMessage);
    connect(s, &QWebSocket::disconnected, this, [this, s]() {
        if (m_client != s) return;
        m_client = nullptr;
        setBound(false, {});
        s->deleteLater();
    });
}

void DocsBridge::onMessage(const QString& msg)
{
    const QJsonObject o = QJsonDocument::fromJson(msg.toUtf8()).object();
    const QString type = o.value("type").toString();
    if (type == "hello") {
        if (o.value("token").toString() == pairingToken()) { m_authed = true; return; }
        LOG_WARN("DocsBridge rejected wrong pairing token");
        m_client->close(QWebSocketProtocol::CloseCodePolicyViolated);
        return;
    }
    if (!m_authed) return;
    if (type == "bound") setBound(true, o.value("title").toString());
    else if (type == "unbound") setBound(false, {});
}

QString DocsBridge::pairingToken()
{
    QSettings s(ConfigManager::configFilePath(), QSettings::IniFormat);
    QString token = s.value("docs/pairingToken").toString();
    if (token.isEmpty()) {
        QByteArray raw(16, 0);
        QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(raw.data()), raw.size() / 4);
        token = QString::fromLatin1(raw.toHex());
        s.setValue("docs/pairingToken", token);
    }
    return token;
}

void DocsBridge::setBound(bool bound, const QString& title)
{
    if (m_bound == bound && m_title == title) return;
    m_bound = bound;
    m_title = title;
    emit boundChanged(bound, title);
}

bool DocsBridge::sendText(const QString& text)
{
    if (!m_authed || !m_bound || !m_client) return false;
    m_client->sendTextMessage(QString::fromUtf8(DocsBridgeProtocol::textMessage(text)));
    return true;
}
