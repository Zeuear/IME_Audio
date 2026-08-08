#include "SystemAudioEndpointController.h"

#ifndef _WIN32
// 非 Windows 平台：不切换系统音频端点，全部空实现，保证跨平台编译与调用安全。
// 用编译器内建宏 _WIN32（Qt-free 文件不应依赖 Q_OS_WIN）。
// 切换类方法返回 true（no-op 成功，符合 06d 验收）；id 获取返回空串。
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

// 未文档化但长期稳定的策略配置接口，用于切换系统默认音频端点。
struct IPolicyConfig : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(REFIID, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(REFIID, UINT32, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(REFIID) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(REFIID, void*, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(REFIID, UINT32, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(REFIID, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(REFIID, UINT32*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(REFIID, UINT32) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(REFIID, DWORD, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(REFIID, DWORD, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(LPCWSTR wszDeviceId, ERole eRole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(LPCWSTR, BOOL) = 0;
};
static const GUID IID_IPolicyConfig = {0xf8679f50, 0x850a, 0x41cf, 0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8};
static const GUID CLSID_PolicyConfigClient = {0x870af99c, 0x171d, 0x4f1e, 0x80, 0x47, 0x1e, 0x7c, 0xac, 0x8e, 0x4a, 0x27};
static const GUID IID_IMMDeviceEnumerator = {0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6};
static const GUID CLSID_MMDeviceEnumerator = {0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e};

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
    if (SUCCEEDED(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                   IID_IMMDeviceEnumerator, (void**)&pEnum)) && pEnum) {
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

// 切换系统默认端点到 endpointId（三角色全切）。返回是否全部 SetDefaultEndpoint 成功。
bool applyDefaultEndpoint(const std::string& endpointId, EDataFlow /*flow*/) {
    if (endpointId.empty()) return false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
    bool ok = false;
    IPolicyConfig* pPolicy = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_PolicyConfigClient, nullptr, CLSCTX_ALL,
                                   IID_IPolicyConfig, (void**)&pPolicy)) && pPolicy) {
        std::wstring wid = toWide(endpointId);
        // 三角色都切，确保系统所有声音走该设备（输出/输入统一处理）
        HRESULT h1 = pPolicy->SetDefaultEndpoint(wid.c_str(), eConsole);
        HRESULT h2 = pPolicy->SetDefaultEndpoint(wid.c_str(), eMultimedia);
        HRESULT h3 = pPolicy->SetDefaultEndpoint(wid.c_str(), eCommunications);
        pPolicy->Release();
        ok = SUCCEEDED(h1) && SUCCEEDED(h2) && SUCCEEDED(h3);
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
