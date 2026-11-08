#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <functional>

#include "utils/DownloadManager.h"

// 本机假服务器：路径以 slow 开头的请求，先发响应头和一小段，然后每 20ms 再发一点，永远发不完；
// 其他路径立刻发完一个小文件。用来验证"取消一组下载之后，队列不会卡住"。
namespace {
class FakeServer : public QObject {
public:
    FakeServer() {
        m_server.listen(QHostAddress::LocalHost);
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket* s = m_server.nextPendingConnection()) {
                s->setParent(this);
                connect(s, &QTcpSocket::readyRead, this, [this, s]() { onRequest(s); });
            }
        });
    }
    QUrl url(const QString& path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/%2").arg(m_server.serverPort()).arg(path));
    }

private:
    void onRequest(QTcpSocket* s) {
        const QByteArray req = s->readAll();
        if (m_handled.contains(s)) return;
        m_handled.insert(s);
        if (req.contains("GET /slow")) {
            s->write("HTTP/1.1 200 OK\r\nContent-Length: 100000000\r\nConnection: close\r\n\r\n");
            s->write(QByteArray(2000, 'x'));
            auto* t = new QTimer(s);
            t->setInterval(20);
            connect(t, &QTimer::timeout, s, [s]() { s->write(QByteArray(2000, 'x')); });
            t->start();
        } else {
            const QByteArray body(300, 'y');
            s->write("HTTP/1.1 200 OK\r\nContent-Length: 300\r\nConnection: close\r\n\r\n" + body);
            s->disconnectFromHost();
        }
    }
    QTcpServer m_server;
    QSet<QTcpSocket*> m_handled;
};

bool waitFor(const std::function<bool()>& cond, int ms = 8000)
{
    QElapsedTimer t;
    t.start();
    while (!cond() && t.elapsed() < ms) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return cond();
}
} // namespace

class DownloadCancelTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!qApp) {
            static int argc = 0;
            static char* argv[] = { nullptr };
            new QCoreApplication(argc, argv);
        }
    }
};

TEST_F(DownloadCancelTest, CancellingAnActiveGroupReleasesTheQueueForTheNextGroup) {
    FakeServer server;
    QTemporaryDir dir;
    QNetworkAccessManager nam;
    DownloadManager mgr(&nam);

    QList<QPair<QString, bool>> finished;
    QObject::connect(&mgr, &DownloadManager::groupFinished, &mgr,
                     [&](const QString& id, bool ok) { finished.append({ id, ok }); });
    bool progressed = false;
    QObject::connect(&mgr, &DownloadManager::groupFileProgress, &mgr,
                     [&](const QString& id, const QString&, const QString&, qint64 r, qint64, int) {
                         if (id == "A" && r > 0) progressed = true;
                     });

    mgr.addGroupTask("A", "A", server.url("slow1"), dir.filePath("a1.bin"));
    mgr.addGroupTask("A", "A", server.url("slow2"), dir.filePath("a2.bin"));
    mgr.addGroupTask("B", "B", server.url("fast"), dir.filePath("b.bin"));
    ASSERT_TRUE(waitFor([&] { return progressed; })) << "第一个文件应该已经开始下载";

    mgr.cancelGroup("A");

    EXPECT_TRUE(mgr.tasksInGroup("A").isEmpty()) << "取消后这一组不能还留着任务，否则 isInstalling 永远为真";
    // 取消不能让下载队列卡住：后面排队的 B 组要能下完
    ASSERT_TRUE(waitFor([&] {
        for (const auto& f : finished) if (f.first == "B") return true;
        return false;
    })) << "取消 A 组之后 B 组没有继续下载";

    bool bOk = false;
    int aCount = 0;
    for (const auto& f : finished) {
        if (f.first == "B") bOk = f.second;
        if (f.first == "A") ++aCount;
    }
    EXPECT_TRUE(bOk);
    EXPECT_EQ(aCount, 1) << "A 组只报告一次结束";
    EXPECT_TRUE(QFileInfo::exists(dir.filePath("b.bin")));
    EXPECT_FALSE(QFileInfo::exists(dir.filePath("a1.bin.part"))) << "取消后不留半截文件";
}

TEST_F(DownloadCancelTest, CancellingAQueuedGroupLeavesNoTasksBehind) {
    FakeServer server;
    QTemporaryDir dir;
    QNetworkAccessManager nam;
    DownloadManager mgr(&nam);

    mgr.addGroupTask("A", "A", server.url("slow1"), dir.filePath("a1.bin"));   // 占住下载通道
    mgr.addGroupTask("Q", "Q", server.url("fast"), dir.filePath("q.bin"));      // 排队中
    mgr.cancelGroup("Q");
    EXPECT_TRUE(mgr.tasksInGroup("Q").isEmpty());
    mgr.cancelGroup("A");
    EXPECT_TRUE(mgr.tasksInGroup("A").isEmpty());
}

TEST_F(DownloadCancelTest, ADownloadCanStartAgainAfterItWasCancelled) {
    FakeServer server;
    QTemporaryDir dir;
    QNetworkAccessManager nam;
    DownloadManager mgr(&nam);

    bool progressed = false, done = false;
    QObject::connect(&mgr, &DownloadManager::groupFileProgress, &mgr,
                     [&](const QString&, const QString&, const QString&, qint64 r, qint64, int) { if (r > 0) progressed = true; });
    QObject::connect(&mgr, &DownloadManager::groupFinished, &mgr, [&](const QString& id, bool success) {
        if (id == "R" && success) done = true;
    });

    mgr.addGroupTask("R", "R", server.url("slow1"), dir.filePath("r.bin"));
    ASSERT_TRUE(waitFor([&] { return progressed; }));
    mgr.cancelGroup("R");
    ASSERT_TRUE(mgr.tasksInGroup("R").isEmpty());

    // 重试：同一个组名重新下载（这次是快的文件）
    mgr.addGroupTask("R", "R", server.url("fast"), dir.filePath("r.bin"));
    ASSERT_TRUE(waitFor([&] { return done; })) << "取消之后重新下载同一个组没有完成";
}

// ---- 回归：第一个文件在启动时就同步失败（目标路径建不出来）----
// addGroupTask / addTask 在 startNext 之后还会读 task->id，而失败处理已经在 startNext 里同步 delete 了 task，
// 读已释放内存，复制 QString 会改坏堆（Windows 上表现为 0xc0000374 崩溃）

namespace {
QString unwritableDestination(const QTemporaryDir& dir)
{
    QFile blocker(dir.filePath("blocker"));
    blocker.open(QIODevice::WriteOnly);
    blocker.write("x");
    blocker.close();
    return dir.filePath("blocker/sub/x.bin");   // 父路径是文件，目录建不出来，文件也打不开
}
}

TEST_F(DownloadCancelTest, ASynchronouslyFailingGroupFileStillReturnsItsTaskId) {
    FakeServer server;
    QTemporaryDir dir;
    QNetworkAccessManager nam;
    DownloadManager mgr(&nam);

    int failedGroups = 0;
    QStringList errors;
    QObject::connect(&mgr, &DownloadManager::groupFinished, &mgr, [&](const QString&, bool ok) { if (!ok) ++failedGroups; });
    QObject::connect(&mgr, &DownloadManager::groupFileError, &mgr,
                     [&](const QString&, const QString&, const QString&, const QString& e) { errors << e; });

    const QUrl url = server.url("fast");
    const QString dest = unwritableDestination(dir);
    const QString id = mgr.addGroupTask("F", "F", url, dest);

    EXPECT_EQ(id, url.toString() + "|" + dest);
    EXPECT_EQ(failedGroups, 1);
    ASSERT_EQ(errors.size(), 1);
    EXPECT_TRUE(mgr.tasksInGroup("F").isEmpty());
}

TEST_F(DownloadCancelTest, ASynchronouslyFailingSingleTaskStillReturnsItsTaskId) {
    FakeServer server;
    QTemporaryDir dir;
    QNetworkAccessManager nam;
    DownloadManager mgr(&nam);

    QString reported;
    const QUrl url = server.url("fast");
    const QString dest = unwritableDestination(dir);
    const QString id = mgr.addTask(url, dest, nullptr, [&](const QString& e) { reported = e; });

    EXPECT_EQ(id, url.toString() + "|" + dest);
    EXPECT_FALSE(reported.isEmpty());
}

TEST_F(DownloadCancelTest, AGroupWhoseEveryFileFailsAtStartEndsOncePerFileAndLeavesNothingBehind) {
    FakeServer server;
    QTemporaryDir dir;
    QNetworkAccessManager nam;
    DownloadManager mgr(&nam);

    int failedGroups = 0;
    QObject::connect(&mgr, &DownloadManager::groupFinished, &mgr, [&](const QString&, bool ok) { if (!ok) ++failedGroups; });
    const QString dest = unwritableDestination(dir);
    for (int i = 0; i < 3; ++i)
        mgr.addGroupTask("G", "G", server.url("fast"), dest + QString::number(i));

    EXPECT_GE(failedGroups, 1);
    EXPECT_TRUE(mgr.tasksInGroup("G").isEmpty());
    // 失败之后下载通道不能被占住：后面的正常下载要能继续
    bool ok = false;
    QObject::connect(&mgr, &DownloadManager::groupFinished, &mgr, [&](const QString& id, bool success) { if (id == "H" && success) ok = true; });
    mgr.addGroupTask("H", "H", server.url("fast"), dir.filePath("h.bin"));
    ASSERT_TRUE(waitFor([&] { return ok; })) << "同步失败之后下载队列卡住了";
}
