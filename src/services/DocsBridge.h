#pragma once
#include <QObject>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

class QWebSocketServer;
class QWebSocket;

// @brief 本机 WebSocket 服务：Chrome 扩展把听写文字写进被绑定的 Google 文档标签页。
// 协议：扩展→程序 hello(token) 后 bound(title) / unbound / ping；程序→扩展 ready / denied / release / text(text)
namespace DocsBridgeProtocol {
inline constexpr quint16 kPort = 38617;
// 对应 extensions/google-docs/manifest.json 的 key
inline const QString kExtensionOrigin = QStringLiteral("chrome-extension://hhpgdlkkbdnlilodnjipmnfbmcpbfeak");

inline bool isAllowedOrigin(const QString& origin)
{
    return origin == kExtensionOrigin || origin == kExtensionOrigin + QLatin1Char('/');
}

// 程序→扩展的状态消息：ready（配对通过）/ denied（令牌不对）/ release（程序要求解除绑定）
inline QByteArray typeMessage(const QString& type)
{
    return QJsonDocument(QJsonObject{{"type", type}}).toJson(QJsonDocument::Compact);
}

inline QByteArray textMessage(const QString& text)
{
    return QJsonDocument(QJsonObject{{"type", "text"}, {"text", text}}).toJson(QJsonDocument::Compact);
}

inline QByteArray tokenHash(const QString& token)
{
    return QCryptographicHash::hash(token.toUtf8(), QCryptographicHash::Sha256).toHex();
}

inline bool tokenMatches(const QString& token, const QByteArray& storedHash)
{
    if (token.isEmpty() || storedHash.isEmpty()) return false;
    const QByteArray h = tokenHash(token);
    if (h.size() != storedHash.size()) return false;
    // 定长比较，不因第一个不同字节提前返回
    char diff = 0;
    for (int i = 0; i < h.size(); ++i) diff |= h[i] ^ storedHash[i];
    return diff == 0;
}
}

class DocsBridge : public QObject {
    Q_OBJECT
public:
    explicit DocsBridge(QObject* parent = nullptr);
    ~DocsBridge() override;

    void setEnabled(bool on);

    // 绑定中，或绑定后连接意外断开（Lost）：两种情况听写结果都不能注入前台，否则会打进别的软件
    bool holdsOutput() const { return m_bound || m_lost; }
    bool isBound() const { return m_bound; }
    bool isLost() const { return m_lost; }
    QString boundTitle() const { return m_title; }
    bool sendText(const QString& text);
    // 用户在程序里停止文档输出：通知扩展解绑，结果恢复注入前台
    void release();

    // 导出扩展时生成新令牌，导出成功后再 saveToken，旧令牌随之作废
    static QString newToken();
    static void saveToken(const QString& token);
    static bool hasToken();

signals:
    void linkChanged();

private:
    void onNewConnection();
    void onMessage(const QString& msg);
    void setLink(bool bound, bool lost, const QString& title);
    // 连接断开时，原先绑定着就进入 Lost
    void dropClient();

    QWebSocketServer* m_server = nullptr;
    QWebSocket* m_client = nullptr;
    bool m_authed = false;
    bool m_bound = false;
    bool m_lost = false;
    QString m_title;
};
