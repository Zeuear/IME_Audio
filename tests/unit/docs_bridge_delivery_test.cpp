#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWebSocket>
#include <functional>
#include <iostream>

#include "services/docs/DocsBridge.h"

namespace {

const QString kToken = QStringLiteral("test-token");

bool waitFor(const std::function<bool()>& done, int timeoutMs = 5000)
{
    QElapsedTimer t;
    t.start();
    while (!done()) {
        if (t.elapsed() > timeoutMs) return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return true;
}

void startBridge(DocsBridge& bridge)
{
    bridge.setTokenHashSource([]() { return DocsBridgeProtocol::tokenHash(kToken); });
    bridge.setEnabled(true, 0);
    ASSERT_NE(bridge.serverPort(), 0);
}

// 只按协议收发、不带去重逻辑的假扩展，用来检查程序这一侧的行为
struct FakeExtension {
    QWebSocket socket{DocsBridgeProtocol::kExtensionOrigin};
    QList<QJsonObject> received;

    explicit FakeExtension(DocsBridge& bridge)
    {
        QObject::connect(&socket, &QWebSocket::textMessageReceived, [this](const QString& m) {
            received << QJsonDocument::fromJson(m.toUtf8()).object();
        });
        socket.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(bridge.serverPort())));
        waitFor([this]() { return socket.state() == QAbstractSocket::ConnectedState; });
        send({{"type", "hello"}, {"token", kToken}});
        waitFor([&bridge]() { return bridge.isConnected(); });
    }
    void send(const QJsonObject& o) { socket.sendTextMessage(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact))); }
    QList<QJsonObject> texts() const
    {
        QList<QJsonObject> out;
        for (const QJsonObject& o : received)
            if (o.value("type") == "text") out << o;
        return out;
    }
};

}   // namespace

TEST(DocsBridgeDelivery, AckedSentencesLeaveTheQueue)
{
    DocsBridge bridge;
    startBridge(bridge);
    FakeExtension ext(bridge);
    ext.send({{"type", "bound"}, {"title", "Doc"}});
    ASSERT_TRUE(waitFor([&]() { return bridge.isBound(); }));

    ASSERT_TRUE(bridge.sendText("a"));
    ASSERT_TRUE(bridge.sendText("b"));
    ASSERT_TRUE(waitFor([&]() { return ext.texts().size() == 2; }));
    EXPECT_EQ(bridge.pendingCount(), 2);
    // ack 是累计的：确认 2 就把 1、2 都出队
    ext.send({{"type", "ack"}, {"id", ext.texts()[1].value("id")}});
    EXPECT_TRUE(waitFor([&]() { return bridge.pendingCount() == 0; }));
}

TEST(DocsBridgeDelivery, UnackedSentencesAreResentInOrderAfterReconnect)
{
    DocsBridge bridge;
    startBridge(bridge);
    auto ext = std::make_unique<FakeExtension>(bridge);
    ext->send({{"type", "bound"}, {"title", "Doc"}});
    ASSERT_TRUE(waitFor([&]() { return bridge.isBound(); }));
    bridge.sendText("one");
    ASSERT_TRUE(waitFor([&]() { return ext->texts().size() == 1; }));
    ext->send({{"type", "ack"}, {"id", ext->texts()[0].value("id")}});
    bridge.sendText("two");
    ASSERT_TRUE(waitFor([&]() { return ext->texts().size() == 2; }));

    ext->socket.close();
    ASSERT_TRUE(waitFor([&]() { return bridge.isLost(); }));
    // 断开期间的句子先存着，不注入前台
    EXPECT_TRUE(bridge.holdsOutput());
    EXPECT_TRUE(bridge.sendText("three"));

    ext = std::make_unique<FakeExtension>(bridge);
    ext->send({{"type", "bound"}, {"title", "Doc"}});
    ASSERT_TRUE(waitFor([&]() { return ext->texts().size() == 2; }));
    EXPECT_EQ(ext->texts()[0].value("text").toString(), "two");
    EXPECT_EQ(ext->texts()[1].value("text").toString(), "three");
    EXPECT_LT(ext->texts()[0].value("id").toDouble(), ext->texts()[1].value("id").toDouble());
}

TEST(DocsBridgeDelivery, InsertFailureKeepsSentenceAndHoldsOutput)
{
    DocsBridge bridge;
    startBridge(bridge);
    FakeExtension ext(bridge);
    ext.send({{"type", "bound"}, {"title", "Doc"}});
    ASSERT_TRUE(waitFor([&]() { return bridge.isBound(); }));
    bool reported = false;
    QObject::connect(&bridge, &DocsBridge::insertFailed, [&reported]() { reported = true; });

    bridge.sendText("x");
    ASSERT_TRUE(waitFor([&]() { return ext.texts().size() == 1; }));
    ext.send({{"type", "insertFailed"}, {"id", ext.texts()[0].value("id")}, {"reason", "boom"}});
    ASSERT_TRUE(waitFor([&]() { return reported; }));
    EXPECT_TRUE(bridge.isLost());
    EXPECT_EQ(bridge.pendingCount(), 1);

    ext.send({{"type", "bound"}, {"title", "Doc"}});
    EXPECT_TRUE(waitFor([&]() { return ext.texts().size() == 2; }));
    EXPECT_EQ(ext.texts()[1].value("text").toString(), "x");
}

TEST(DocsBridgeDelivery, StoppingOutputOrUnbindingDropsTheQueue)
{
    DocsBridge bridge;
    startBridge(bridge);
    FakeExtension ext(bridge);
    ext.send({{"type", "bound"}, {"title", "Doc"}});
    ASSERT_TRUE(waitFor([&]() { return bridge.isBound(); }));
    bridge.sendText("a");
    bridge.release();
    EXPECT_EQ(bridge.pendingCount(), 0);
    EXPECT_FALSE(bridge.holdsOutput());
    EXPECT_FALSE(bridge.sendText("b"));

    ext.send({{"type", "bound"}, {"title", "Doc"}});
    ASSERT_TRUE(waitFor([&]() { return bridge.isBound(); }));
    bridge.sendText("c");
    ext.send({{"type", "unbound"}});
    ASSERT_TRUE(waitFor([&]() { return !bridge.isBound(); }));
    EXPECT_EQ(bridge.pendingCount(), 0);
    EXPECT_FALSE(bridge.holdsOutput());
}

// 端到端：真实的 background.js（Node 里跑，Chrome 接口是假的）连真实的 DocsBridge，
// 随机断线、重启后台脚本、写入失败，核对每一句都恰好写进文档一次且顺序不变
TEST(DocsBridgeDelivery, SoakWithRealExtensionScript)
{
    const QString node = QStandardPaths::findExecutable(QStringLiteral("node"));
    if (node.isEmpty()) GTEST_SKIP() << "node not found";
    const QString harness = QStringLiteral(IME_AUDIO_TEST_DATA_DIR "/../support/docs_extension_harness.mjs");
    const QString script = QStringLiteral(IME_AUDIO_RESOURCES_DIR "/../extensions/google-docs/background.js");
    // 平时只跑一轮 1000 句加一轮溢出；设 IMEAUDIO_DOCS_SOAK_FULL=1 跑完整的 5 轮
    const bool full = qEnvironmentVariableIsSet("IMEAUDIO_DOCS_SOAK_FULL");
    const int kSentences = full ? 1500 : 1000;
    // 最后一轮把队列上限调得很小，故意让积压溢出丢句，检查补发能越过缺口、不卡死
    // 第 5 轮调高重启概率：重启只在没有写入进行中时发生，默认概率下可能一次都碰不上
    struct Run { int seed; int maxPending; const char* restart; };

    const QList<Run> runs = full
        ? QList<Run>{{1, 500, "0.02"}, {2, 500, "0.02"}, {3, 500, "0.02"}, {4, 20, "0.02"}, {5, 500, "0.3"}}
        : QList<Run>{{5, 500, "0.3"}, {4, 20, "0.02"}};

    for (const Run& run : runs) {
        const int seed = run.seed;
        const bool overflowRun = run.maxPending < 500;
        SCOPED_TRACE(seed);
        DocsBridge bridge;
        startBridge(bridge);
        bridge.setMaxPending(run.maxPending);
        QTemporaryDir tmp;
        const QString out = tmp.filePath("doc.jsonl");

        QProcess proc;
        proc.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        proc.start(node, {harness, "--script", script, "--port", QString::number(bridge.serverPort()),
                          "--token", kToken, "--out", out, "--seed", QString::number(seed),
                          "--drop", "0.03", "--restart", run.restart, "--fail", "0.01"});
        ASSERT_TRUE(proc.waitForStarted(5000));

        ASSERT_TRUE(waitFor([&]() { return bridge.isConnected(); }, 10000));
        bridge.requestBindActive();
        ASSERT_TRUE(waitFor([&]() { return bridge.isBound(); }, 10000));

        // 断开后扩展会自己重连并恢复绑定；写入失败后扩展已解绑，模拟用户在窗口里点"用 Chrome 当前标签页"
        QElapsedTimer lastRebind;
        auto rebindIfNeeded = [&]() {
            if (bridge.isLost() && bridge.isConnected() && (!lastRebind.isValid() || lastRebind.elapsed() > 300)) {
                lastRebind.start();
                bridge.requestBindActive();
            }
        };

        QStringList expected;
        for (int i = 1; i <= kSentences; ++i) {
            ASSERT_TRUE(bridge.holdsOutput()) << "output released unexpectedly at sentence " << i;
            const QString text = QStringLiteral("第%1句：测试 \"引号\" 与换行\n").arg(i);
            bridge.sendText(text);
            expected << text;
            rebindIfNeeded();
            QCoreApplication::processEvents();
            // 真实听写几秒一句，积压不会无限涨；溢出那一轮不等，让队列满
            if (!overflowRun)
                waitFor([&]() { rebindIfNeeded(); return bridge.pendingCount() < 200; }, 60000);
        }
        ASSERT_TRUE(waitFor([&]() {
            rebindIfNeeded();
            return bridge.isBound() && bridge.pendingCount() == 0;
        }, 60000)) << "pending=" << bridge.pendingCount();

        proc.write("quit\n");
        proc.waitForFinished(5000);

        QFile f(out);
        ASSERT_TRUE(f.open(QIODevice::ReadOnly));
        QStringList written;
        for (const QByteArray& line : f.readAll().split('\n'))
            if (!line.isEmpty()) written << QJsonDocument::fromJson("[" + line + "]").array().at(0).toString();

        std::cerr << "seed " << seed << ": written " << written.size() << ", dropped " << bridge.droppedCount() << "\n";
        if (!overflowRun) {
            EXPECT_EQ(bridge.droppedCount(), 0);
            ASSERT_EQ(written.size(), expected.size());
            for (int i = 0; i < expected.size(); ++i)
                ASSERT_EQ(written[i], expected[i]) << "first mismatch at sentence " << i + 1;
            continue;
        }
        // 溢出时：写进去的是原序列的有序子序列，没有重复，最后一句一定在，缺的句数不超过丢弃数
        int cursor = 0;
        for (const QString& w : written) {
            while (cursor < expected.size() && expected[cursor] != w) ++cursor;
            ASSERT_LT(cursor, expected.size()) << "out of order or duplicated: " << w.toStdString();
            ++cursor;
        }
        EXPECT_GT(bridge.droppedCount(), 0);
        EXPECT_GE(written.size(), expected.size() - bridge.droppedCount());
        EXPECT_EQ(written.last(), expected.last());
    }
}
