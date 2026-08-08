#include "SystemAudioEndpointController.h"

#ifndef _WIN32
// 非 Windows 平台：不切换系统音频端点，全部空实现，保证跨平台编译与调用安全。
bool SystemAudioEndpointController::setDefaultOutput(const std::string&) { return true; }
bool SystemAudioEndpointController::setDefaultInput(const std::string&) { return true; }
std::string SystemAudioEndpointController::getDefaultOutputId() const { return {}; }
std::string SystemAudioEndpointController::getDefaultInputId() const { return {}; }
void SystemAudioEndpointController::restore() {}

#else
// Windows：通过 Core Audio COM API 切换系统默认音频端点。
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <comdef.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include "PolicyConfig.h"  

namespace {
    // wchar_t 端点 id -> std::string (UTF-8)
    std::string widenToString(LPCWSTR id) {
        if (!id) return {};
        int n = WideCharToMultiByte(CP_UTF8, 0, id, -1, nullptr, 0, nullptr, nullptr);
        if (n <= 0) return {};
        std::string out(n - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, id, -1, &out[0], n, nullptr, nullptr);
        return out;
    }
    std::wstring toWide(const std::string& s) {
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (n <= 0) return {};
        std::wstring out(n - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], n);
        return out;
    }

    // 取当前系统默认端点的 id（eRender=播放, eCapture=输入）。失败返回空串。
    std::string currentDefaultId(EDataFlow flow) {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool needUninit = SUCCEEDED(hr);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return {};
        std::string result;
        IMMDeviceEnumerator* pEnum = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), (void**)&pEnum)) && pEnum) {
            IMMDevice* pDev = nullptr;
            if (SUCCEEDED(pEnum->GetDefaultAudioEndpoint(flow, eConsole, &pDev)) && pDev) {
                LPWSTR id = nullptr;
                if (SUCCEEDED(pDev->GetId(&id)) && id) {
                    result = widenToString(id);
                    CoTaskMemFree(id);
                }
                pDev->Release();
            }
            pEnum->Release();
        }
        if (needUninit) CoUninitialize();
        return result;
    }

    // 切换系统默认端点到 endpointId（三角色全切）。
    // 使用未文档化但社区广泛验证可用的 IPolicyConfig::SetDefaultEndpoint。
    // flow 参数在这个接口下其实用不到（端点类型由 endpointId 自身决定），
    // 保留是为了兼容原有调用方式和 log 输出。
    bool applyDefaultEndpoint(const std::string& endpointId, EDataFlow /*flow*/) {
        if (endpointId.empty()) return false;
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool needUninit = SUCCEEDED(hr);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            OutputDebugStringA("[DIAG-SAEC] CoInitializeEx failed\n");
            return false;
        }

        bool ok = false;
        std::wstring wid = toWide(endpointId);

        // 优先尝试 IPolicyConfig（Win7+ 通用）
        IPolicyConfig* pPolicyConfig = nullptr;
        HRESULT hrCreate = CoCreateInstance(__uuidof(CPolicyConfigClient), nullptr, CLSCTX_ALL,
            __uuidof(IPolicyConfig), (void**)&pPolicyConfig);
        {
            char buf[256];
            snprintf(buf, sizeof(buf), "[DIAG-SAEC] CoCreateInstance IPolicyConfig hr=0x%08X p=%p id='%s'\n",
                (unsigned)hrCreate, (void*)pPolicyConfig, endpointId.c_str());
            OutputDebugStringA(buf);
        }
        if (SUCCEEDED(hrCreate) && pPolicyConfig) {
            HRESULT h1 = pPolicyConfig->SetDefaultEndpoint(wid.c_str(), eConsole);
            HRESULT h2 = pPolicyConfig->SetDefaultEndpoint(wid.c_str(), eMultimedia);
            HRESULT h3 = pPolicyConfig->SetDefaultEndpoint(wid.c_str(), eCommunications);
            pPolicyConfig->Release();
            {
                char buf[256];
                snprintf(buf, sizeof(buf), "[DIAG-SAEC] IPolicyConfig::SetDefaultEndpoint h1=0x%08X h2=0x%08X h3=0x%08X\n",
                    (unsigned)h1, (unsigned)h2, (unsigned)h3);
                OutputDebugStringA(buf);
            }
            // 至少一个角色成功就算切换生效（有些设备/驱动不完整支持三种角色）
            ok = SUCCEEDED(h1) || SUCCEEDED(h2) || SUCCEEDED(h3);
        }
        else {
            OutputDebugStringA("[DIAG-SAEC] IPolicyConfig unavailable, falling back to IPolicyConfigVista\n");
            // 回退：极少数系统 IPolicyConfig 不可用时，尝试 Vista 版接口
            IPolicyConfigVista* pPolicyConfigVista = nullptr;
            HRESULT hrCreateVista = CoCreateInstance(__uuidof(CPolicyConfigVistaClient), nullptr, CLSCTX_ALL,
                __uuidof(IPolicyConfigVista), (void**)&pPolicyConfigVista);
            if (SUCCEEDED(hrCreateVista) && pPolicyConfigVista) {
                HRESULT h1 = pPolicyConfigVista->SetDefaultEndpoint(wid.c_str(), eConsole);
                HRESULT h2 = pPolicyConfigVista->SetDefaultEndpoint(wid.c_str(), eMultimedia);
                HRESULT h3 = pPolicyConfigVista->SetDefaultEndpoint(wid.c_str(), eCommunications);
                pPolicyConfigVista->Release();
                ok = SUCCEEDED(h1) || SUCCEEDED(h2) || SUCCEEDED(h3);
                char buf[256];
                snprintf(buf, sizeof(buf), "[DIAG-SAEC] IPolicyConfigVista::SetDefaultEndpoint h1=0x%08X h2=0x%08X h3=0x%08X\n",
                    (unsigned)h1, (unsigned)h2, (unsigned)h3);
                OutputDebugStringA(buf);
            }
            else {
                char buf[256];
                snprintf(buf, sizeof(buf), "[DIAG-SAEC] IPolicyConfigVista also unavailable hr=0x%08X\n", (unsigned)hrCreateVista);
                OutputDebugStringA(buf);
            }
        }

        if (needUninit) CoUninitialize();
        return ok;
    }
} // namespace

bool SystemAudioEndpointController::setDefaultOutput(const std::string& endpointId) {
    if (endpointId.empty()) return false;
    ensureSaved();
    return applyDefaultEndpoint(endpointId, eRender);
}

bool SystemAudioEndpointController::setDefaultInput(const std::string& endpointId) {
    if (endpointId.empty()) return false;
    ensureSaved();
    return applyDefaultEndpoint(endpointId, eCapture);
}

void SystemAudioEndpointController::ensureSaved() {
    if (m_hasSaved) return;
    m_savedOutputId = currentDefaultId(eRender);
    m_savedInputId = currentDefaultId(eCapture);
    m_hasSaved = true;
}

std::string SystemAudioEndpointController::getDefaultOutputId() const {
    return currentDefaultId(eRender);
}
std::string SystemAudioEndpointController::getDefaultInputId() const {
    return currentDefaultId(eCapture);
}

void SystemAudioEndpointController::restore() {
    if (!m_hasSaved) return;
    if (!m_savedOutputId.empty()) applyDefaultEndpoint(m_savedOutputId, eRender);
    if (!m_savedInputId.empty()) applyDefaultEndpoint(m_savedInputId, eCapture);
    m_hasSaved = false;
}
#endif