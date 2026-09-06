#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>

#include "utils/SingleInstanceGuard.h"

namespace {
// 让事件循环跑一小会儿，把挂起的连接派发掉
void pumpEvents(int ms = 500) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}
} // namespace

class SingleInstanceGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!qApp) {
            static int argc = 0;
            static char* argv[] = { nullptr };
            m_app = new QCoreApplication(argc, argv);
        }
        ASSERT_TRUE(m_dir.isValid());
    }

    QString dataDir() const { return m_dir.path(); }

    QCoreApplication* m_app = nullptr;
    QTemporaryDir m_dir;
};

TEST_F(SingleInstanceGuardTest, FirstInstanceAcquires) {
    SingleInstanceGuard guard(dataDir());
    EXPECT_TRUE(guard.acquireOrNotifyExisting());
}

TEST_F(SingleInstanceGuardTest, SecondInstanceIsRejected) {
    SingleInstanceGuard first(dataDir());
    ASSERT_TRUE(first.acquireOrNotifyExisting());

    SingleInstanceGuard second(dataDir());
    EXPECT_FALSE(second.acquireOrNotifyExisting());
}

TEST_F(SingleInstanceGuardTest, LockIsReusableAfterOwnerReleases) {
    {
        SingleInstanceGuard first(dataDir());
        ASSERT_TRUE(first.acquireOrNotifyExisting());
    }

    // owner 退出后必须能重新占用，否则崩溃过一次就再也起不来
    SingleInstanceGuard next(dataDir());
    EXPECT_TRUE(next.acquireOrNotifyExisting());
}

TEST_F(SingleInstanceGuardTest, DifferentDataDirsDoNotConflict) {
    QTemporaryDir other;
    ASSERT_TRUE(other.isValid());

    SingleInstanceGuard a(dataDir());
    SingleInstanceGuard b(other.path());
    EXPECT_TRUE(a.acquireOrNotifyExisting());
    EXPECT_TRUE(b.acquireOrNotifyExisting());
}

TEST_F(SingleInstanceGuardTest, ServerNameIsStableAcrossProcesses) {
    // 固定值断言：换成 qHash 之类每进程重新播种的哈希会让这里失败。
    // 两个进程算出不同名字时通知通道会静默失效，而进程内测试全都照常通过。
    EXPECT_EQ(SingleInstanceGuard::serverNameForDataDir("C:/Users/test/AppData/Local/ImeAudio"),
              QString("ImeAudio-4967b7b6cc7f5067"));
    EXPECT_NE(SingleInstanceGuard::serverNameForDataDir("/home/a/.local/share/ImeAudio"),
              SingleInstanceGuard::serverNameForDataDir("/home/b/.local/share/ImeAudio"));
}

TEST_F(SingleInstanceGuardTest, OwnerIsNotifiedOncePerLaunchAttempt) {
    SingleInstanceGuard owner(dataDir());
    ASSERT_TRUE(owner.acquireOrNotifyExisting());

    int notifications = 0;
    QObject::connect(&owner, &SingleInstanceGuard::anotherInstanceStarted,
                     &owner, [&] { ++notifications; });

    SingleInstanceGuard first(dataDir());
    ASSERT_FALSE(first.acquireOrNotifyExisting());
    pumpEvents();
    EXPECT_EQ(notifications, 1);

    // 每次启动尝试恰好一次，不能重复计数
    SingleInstanceGuard second(dataDir());
    ASSERT_FALSE(second.acquireOrNotifyExisting());
    pumpEvents();
    EXPECT_EQ(notifications, 2);
}
