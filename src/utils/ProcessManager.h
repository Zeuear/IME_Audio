#pragma once
#include <QString>
#include <QObject>
#include <QQueue>
#include <QProcess>
#include <QThread>
#include <QElapsedTimer>
#include <QDir>
#include "ExtractTool.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#include <shellapi.h>
#endif



enum class TaskResult {
    Success,
    Failed,
    Cancelled
};

class BaseTask : public QObject {
    Q_OBJECT
public:
    virtual ~BaseTask() = default;
    virtual void run() = 0;

signals:
    void taskFinished(TaskResult result, const QString& message);
    // 解压进度
    void extractStarted(int totalLines);
    void extractProgress(int current, int total);
    void extractFinished(bool success);
};


class ProcessTask : public BaseTask {
    Q_OBJECT
public:
    ProcessTask(const QString& program, const QStringList& args, QObject* parent = nullptr)
        : BaseTask(), m_program(program), m_args(args) {
    }

    void run() override {
        m_process = new QProcess(this);
        connect(m_process, &QProcess::finished, this, &ProcessTask::onFinished);
        m_process->start(m_program, m_args);
    }

private slots:
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitCode == 0 && exitStatus == QProcess::NormalExit)
            emit taskFinished(TaskResult::Success, "Process finished successfully");
        else
            emit taskFinished(TaskResult::Failed, QString("Process failed with code %1").arg(exitCode));
    }

private:
    QProcess* m_process = nullptr;
    QString m_program;
    QStringList m_args;
};


class ElevatedProcessTask : public BaseTask {
    Q_OBJECT
public:
    ElevatedProcessTask(const QString& program, const QStringList& args, QObject* parent = nullptr)
        : BaseTask(), m_program(program), m_args(args) {
    }

    ~ElevatedProcessTask() override {
        if (m_thread) {
            m_thread->quit();
            m_thread->wait();
        }
    }

    void run() override {
#ifdef Q_OS_WIN
        m_thread = new QThread();
        // 用一个 QObject worker 把阻塞逻辑丢进子线程执行
        connect(m_thread, &QThread::started, this, &ElevatedProcessTask::runElevatedBlocking);
        connect(this, &ElevatedProcessTask::elevatedFinished, m_thread, &QThread::quit);
        connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
        this->moveToThread(m_thread);
        m_thread->start();
#else
        emit taskFinished(TaskResult::Failed, "Elevated install only supported on Windows");
#endif
    }

signals:
    void elevatedFinished();
    void installProgress(const QString& msg);

private slots:
#ifdef Q_OS_WIN
    void runElevatedBlocking() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool comInitialized = SUCCEEDED(hr);

        std::wstring wExe = QDir::toNativeSeparators(m_program).toStdWString();
        std::wstring wArgs = m_args.join(" ").toStdWString();

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = L"runas";
        sei.lpFile = wExe.c_str();
        sei.lpParameters = wArgs.c_str();
        sei.nShow = SW_HIDE;

        if (!ShellExecuteExW(&sei)) {
            DWORD err = GetLastError();
            QString errMsg;
            if (err == ERROR_CANCELLED) {
                emit taskFinished(TaskResult::Cancelled, "User declined UAC elevation");
            }
            else {
                errMsg = QString("ShellExecuteEx failed with error %1").arg(err);
                LOG_WARN(errMsg);
                emit taskFinished(TaskResult::Failed, errMsg);
            }
            emit elevatedFinished();
            return;
        }

        QElapsedTimer elapsed;
        elapsed.start();
        const DWORD pollIntervalMs = 1000;
        DWORD waitResult;

        do {
            waitResult = WaitForSingleObject(sei.hProcess, pollIntervalMs);

            if (waitResult == WAIT_TIMEOUT) {
                qint64 secs = elapsed.elapsed() / 1000;
                QString msg = QString("Installing... elapsed %1s").arg(secs);
                emit installProgress(msg);
                LOG_INFO(msg);
            }
        } while (waitResult == WAIT_TIMEOUT);


        DWORD exitCode = 1;
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);

        if (comInitialized) CoUninitialize();

        if (exitCode == 0) {
            emit installProgress("Installation completed successfully.");
            emit taskFinished(TaskResult::Success, "Elevated process finished successfully");
        }
        else {
            QString msg = QString("Elevated process failed with code %1").arg(exitCode);
            emit installProgress(msg);
            emit taskFinished(TaskResult::Failed, QString("Elevated process failed with code %1").arg(exitCode));
        }
        emit elevatedFinished();
    }
#endif

private:
    QString m_program;
    QStringList m_args;
    QThread* m_thread = nullptr;
};


class ExtractTask : public BaseTask {
    Q_OBJECT
public:
    ExtractTask(const QString& archive, const QString& target, QObject* parent = nullptr)
        : BaseTask(), m_archive(archive), m_target(target) {}

    void run() override {
        bool ok = ExtractTool::extractAll(m_archive, m_target, true,
            [this](const ExtractProgress& p) {
                if (p.total > 0 && p.current == 1) emit extractStarted(p.total);
                emit extractProgress(p.current, p.total);
            });
        emit extractFinished(ok);
        if (ok) emit taskFinished(TaskResult::Success, "Extraction successful");
        else emit taskFinished(TaskResult::Failed, "Extraction failed");
    }

private:
    QString m_archive;
    QString m_target;
};


class ExtractExTask : public BaseTask {
    Q_OBJECT
public:
    ExtractExTask(const QString& archive, const ExtractOptions& opts, QObject* parent = nullptr)
        : BaseTask(), m_archive(archive), m_opts(opts) {
    }

    void run() override {
        m_opts.onProgress = [this](const ExtractProgress& p) {
            if (p.total > 0 && p.current == 1) emit extractStarted(p.total);
            emit extractProgress(p.current, p.total);
        };
        bool ok = ExtractTool::extractAndDeploy(m_archive, m_opts);
        emit extractFinished(ok);
        if (ok) emit taskFinished(TaskResult::Success, "Extraction successful");
        else emit taskFinished(TaskResult::Failed, "Extraction failed");
    }

private:
    QString m_archive;
    ExtractOptions m_opts;
};




class TaskQueueManager : public QObject {
    Q_OBJECT
public:
    explicit TaskQueueManager(QObject* parent = nullptr) : QObject(parent) {}

    void addTask(BaseTask* task) {
        m_queue.enqueue(task);
        if (!m_running) {
            startNext();
        }
    }

signals:
    void allTasksFinished(bool allSuccess, const QString& summary);

private:
    void startNext() {
        if (m_queue.isEmpty()) {
            emit allTasksFinished(m_allSuccessful, m_summary);
            m_running = false;
            return;
        }

        m_running = true;
        m_currentTask = m_queue.dequeue();

        // task 必须无 parent 才能 moveToThread；若调用方误设了 parent，先清掉，
        // 避免 "QObject with parent cannot be moved" 导致 move 失败、任务仍跑在主线程。
        m_currentTask->setParent(nullptr);

        // 把 task 丢进独立 worker 线程执行，真正脱离调用线程（主线程）
        QThread* thread = new QThread(this);
        m_currentTask->moveToThread(thread);

        connect(thread, &QThread::started, m_currentTask, &BaseTask::run);
        connect(m_currentTask, &BaseTask::taskFinished, this, &TaskQueueManager::onTaskDone);
        // 关键：用直接连接（无接收者对象 → 在发送者线程即 worker 线程执行）调用 thread->quit()，
        // 让 worker 线程自己退出自身的事件循环，不依赖主线程事件循环投递。
        // 否则 quit() 会被排队到主线程，一旦主线程阻塞（如同步调用方未运行事件循环）
        // 就永远收不到，导致 worker 线程 exec() 死等、析构时死锁。
        connect(m_currentTask, &BaseTask::taskFinished, [thread]() { thread->quit(); });
        connect(thread, &QThread::finished, m_currentTask, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();
    }

    void onTaskDone(TaskResult result, const QString& message) {
        if (result != TaskResult::Success) {
            m_allSuccessful = false;
            m_summary += message + "; ";
        }
        // 线程已 quit，task 将在 finished 时 deleteLater；直接启动下一个
        startNext();
    }
        
private:
    QQueue<BaseTask*> m_queue;
    BaseTask* m_currentTask = nullptr;
    bool m_running = false;
    bool m_allSuccessful = true;
    QString m_summary;
};