#pragma once
#include <QString>
#include <QObject>
#include <QQueue>
#include <QProcess>
#include <QThread>
#include <QElapsedTimer>
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
                LOG_ERROR(errMsg);
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
        bool ok = ExtractTool::extractAll(m_archive, m_target, true, [](const QString& line) { LOG_DEBUG(line); });
        
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
        bool ok = ExtractTool::extractAndDeploy(m_archive, m_opts);

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

        connect(m_currentTask, &BaseTask::taskFinished, this, &TaskQueueManager::onTaskDone);
        m_currentTask->run();
    }

    void onTaskDone(TaskResult result, const QString& message) {
        m_currentTask->deleteLater();

        if (result != TaskResult::Success) {
            m_allSuccessful = false;
            m_summary += message + "; ";
            // 如果希望“一旦失败就停止”，就在这里 return;
            // 如果希望“失败了也继续跑下一个”，就直接 startNext();
        }

        startNext();
    }
        
private:
    QQueue<BaseTask*> m_queue;
    BaseTask* m_currentTask = nullptr;
    bool m_running = false;
    bool m_allSuccessful = true;
    QString m_summary;
};