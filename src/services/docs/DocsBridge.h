#pragma once
#include <QObject>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <functional>

class QWebSocketServer;
class QWebSocket;

// @brief 本机 WebSocket 服务：Chrome 扩展把听写文字写进被绑定的 Google 文档标签页。
// 协议：扩展→程序 hello(token) 后 bound(title) / unbound / bindFailed(reason) / ack(id) / insertFailed(id, reason) / ping；
// 程序→扩展 ready / denied / release / newDoc / bindActive / resume(session, from) / text(session, id, text)
// 送达：每句带递增 id，扩展写进文档后回 ack；没确认的句子在重新绑定后按顺序补发，
// 扩展按 (session, id) 去重且只接受下一个连续 id，所以不丢、不重、不乱序。
// 队列满时丢最早的句子，补发前先发 resume 告诉扩展从哪句接着写，否则缺口会让扩展一直等下去。
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

inline QByteArray textMessage(const QString& session, quint64 id, const QString& text)
{
    return QJsonDocument(QJsonObject{{"type", "text"}, {"session", session}, {"id", double(id)}, {"text", text}})
        .toJson(QJsonDocument::Compact);
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

    void setEnabled(bool on, quint16 port = DocsBridgeProtocol::kPort);
    quint16 serverPort() const;
    // 测试用：不读用户配置里的令牌哈希
    void setTokenHashSource(std::function<QByteArray()> source) { m_tokenHash = std::move(source); }
    // 扩展已连上并通过令牌校验
    bool isConnected() const { return m_authed; }

    // 绑定中，或绑定后连接意外断开（Lost）：两种情况听写结果都不能注入前台，否则会打进别的软件
    bool holdsOutput() const { return m_bound || m_lost; }
    bool isBound() const { return m_bound; }
    bool isLost() const { return m_lost; }
    QString boundTitle() const { return m_title; }
    // 绑定或断开期间都接收：先进待确认队列，能发就发，断开时等重新绑定后补发。未持有输出时返回 false
    bool sendText(const QString& text);
    int pendingCount() const { return m_outbox.size(); }
    // 队列满被丢弃的句数
    int droppedCount() const { return m_dropped; }
    void setMaxPending(int n) { m_maxPending = n; }
    // 用户在程序里停止文档输出：通知扩展解绑，结果恢复注入前台
    void release();
    // 让扩展新建一个 Google 文档并绑定 / 绑定 Chrome 当前标签页；扩展没连上返回 false
    bool requestNewDoc();
    bool requestBindActive();

    // 导出扩展时生成新令牌，导出成功后再 saveToken，旧令牌随之作废
    static QString newToken();
    static void saveToken(const QString& token);
    static bool hasToken();
    static bool isCurrentToken(const QString& token);

signals:
    void linkChanged();
    // reason：notDocs（当前标签页不是 Google 文档）或扩展给出的错误信息
    void bindFailed(const QString& reason);
    // 扩展写入文档失败，句子留在队列里，重新绑定后补写
    void insertFailed(const QString& reason);

private:
    void onNewConnection();
    void onMessage(const QString& msg);
    void setLink(bool bound, bool lost, const QString& title);
    // 连接断开时，原先绑定着就进入 Lost
    void dropClient();
    bool sendCommand(const QString& type);
    bool canSend() const { return m_client && m_authed && m_bound; }
    void flushOutbox();
    void clearOutbox(const char* why);

    struct Pending {
        quint64 id;
        QString text;
    };
    static constexpr int kMaxOutbox = 500;
    int m_maxPending = kMaxOutbox;
    int m_dropped = 0;

    QWebSocketServer* m_server = nullptr;
    QWebSocket* m_client = nullptr;
    bool m_authed = false;
    bool m_bound = false;
    bool m_lost = false;
    QString m_title;
    std::function<QByteArray()> m_tokenHash;
    QList<Pending> m_outbox;
    quint64 m_nextId = 1;
    // 程序每次启动换新，扩展据此知道 id 重新从 1 开始
    QString m_session;
};
