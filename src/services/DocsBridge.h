#pragma once
#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

class QWebSocketServer;
class QWebSocket;

// @brief 本机 WebSocket 服务：Chrome 扩展把听写文字写进被绑定的 Google 文档标签页。
// 协议：扩展→程序 hello(token) 后 bound(title) / unbound；程序→扩展 text(text)
namespace DocsBridgeProtocol {
inline constexpr quint16 kPort = 38617;
// 对应 extensions/google-docs/manifest.json 的 key
inline const QString kExtensionOrigin = QStringLiteral("chrome-extension://hhpgdlkkbdnlilodnjipmnfbmcpbfeak");

inline bool isAllowedOrigin(const QString& origin)
{
    return origin == kExtensionOrigin || origin == kExtensionOrigin + QLatin1Char('/');
}

inline QByteArray textMessage(const QString& text)
{
    return QJsonDocument(QJsonObject{{"type", "text"}, {"text", text}}).toJson(QJsonDocument::Compact);
}
}

class DocsBridge : public QObject {
    Q_OBJECT
public:
    explicit DocsBridge(QObject* parent = nullptr);
    ~DocsBridge() override;

    void setEnabled(bool on);
    bool isBound() const { return m_bound; }
    QString boundTitle() const { return m_title; }
    bool sendText(const QString& text);

    // 首次调用随机生成并存进配置
    static QString pairingToken();

signals:
    void boundChanged(bool bound, const QString& title);

private:
    void onNewConnection();
    void onMessage(const QString& msg);
    void setBound(bool bound, const QString& title);

    QWebSocketServer* m_server = nullptr;
    QWebSocket* m_client = nullptr;
    bool m_authed = false;
    bool m_bound = false;
    QString m_title;
};
