#include "utils/FileVersion.h"
#include <windows.h>
#include <vector>

#pragma comment(lib, "version.lib")

QString fileVersion(const QString& path)
{
    const std::wstring wpath = path.toStdWString();
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(wpath.c_str(), &handle);
    if (size == 0) return QString();

    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(wpath.c_str(), 0, size, data.data())) return QString();

    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &len) || !info) return QString();

    return QString("%1.%2.%3.%4")
        .arg(HIWORD(info->dwFileVersionMS)).arg(LOWORD(info->dwFileVersionMS))
        .arg(HIWORD(info->dwFileVersionLS)).arg(LOWORD(info->dwFileVersionLS));
}
