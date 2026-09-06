#include "../../utils/ProcessManager.h"
#include "../../utils/Logger.h"

#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>

// 通过 UAC ("runas") 提权启动安装程序并同步等待其结束。
// 本函数运行在 ElevatedProcessTask 自己的工作线程上，允许阻塞。
void ElevatedProcessTask::runElevatedBlocking() {
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
        emit taskFinished(TaskResult::Failed, msg);
    }
    emit elevatedFinished();
}
