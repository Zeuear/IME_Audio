#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>

#include "utils/ProcessManager.h"

// 模拟一个耗时的 task：run() 内 sleep，用来检测 addTask 是否阻塞调用线程
class SleepTask : public BaseTask {
    Q_OBJECT
public:
    explicit SleepTask(int ms, QObject* parent = nullptr) : BaseTask(), m_ms(ms) {}
    void run() override {
        // 在 worker 线程内用本地事件循环模拟耗时（标准安全做法），
        // 保证 taskFinished 发出时线程事件循环已就绪，thread->quit() 能正常退出。
        QEventLoop loop;
        QTimer::singleShot(m_ms, &loop, &QEventLoop::quit);
        loop.exec();
        emit taskFinished(TaskResult::Success, "ok");
    }
private:
    int m_ms;
};

// 立即完成的 task
class InstantTask : public BaseTask {
    Q_OBJECT
public:
    explicit InstantTask(QObject* parent = nullptr) : BaseTask() {}
    void run() override { emit taskFinished(TaskResult::Success, "ok"); }
};

class TaskQueueAsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!qApp) {
            static int argc = 0;
            static char* argv[] = { nullptr };
            m_app = new QCoreApplication(argc, argv);
        }
    }
    QCoreApplication* m_app = nullptr;
};

// 不变量 1: addTask 不阻塞调用线程（task 在 worker 线程异步执行）
TEST_F(TaskQueueAsyncTest, AddTaskReturnsImmediatelyDoesNotBlock) {
    TaskQueueManager mgr;

    // 用一个明确耗时的 task 强验证：若 addTask 同步执行，会阻塞 ~200ms
    auto* slow = new SleepTask(200);
    QElapsedTimer t;
    t.start();
    mgr.addTask(slow);
    int elapsedMs = t.elapsed();
    // 若 addTask 同步执行，会阻塞 ~200ms；异步则应远小于此
    EXPECT_LT(elapsedMs, 100) << "addTask blocked the caller thread (synchronous run detected)";

    // 给 worker 线程一点时间跑完（不依赖事件循环，避免测试框架阻塞）
    QThread::msleep(500);
}

// 不变量 2: task 完成后 taskFinished 被触发，且 allTasksFinished 在队列清空后发出
TEST_F(TaskQueueAsyncTest, TaskFinishesAndQueueCompletes) {
    TaskQueueManager mgr;

    bool finished = false;
    bool allDone = false;
    QObject::connect(&mgr, &TaskQueueManager::allTasksFinished,
        [&](bool ok, const QString&) { allDone = true; (void)ok; });

    auto* task = new InstantTask();
    QObject::connect(task, &BaseTask::taskFinished,
        [&](TaskResult r, const QString&) { finished = (r == TaskResult::Success); });

    mgr.addTask(task);

    // 等待异步完成（事件循环驱动 worker 线程的 finished → deleteLater）
    QElapsedTimer drain;
    drain.start();
    while (drain.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 100);
        if (finished && allDone) break;
    }

    EXPECT_TRUE(finished) << "taskFinished was not emitted";
    EXPECT_TRUE(allDone) << "allTasksFinished was not emitted after queue drained";
}

#include "task_queue_async_test.moc"
