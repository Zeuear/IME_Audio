#include "OrtRuntime.h"

#include "onnxruntime_c_api.h"
#include "utils/Logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace OrtRuntime {

const OrtApi* api()
{
    static const OrtApi* cached = []() -> const OrtApi* {
        const OrtApiBase* base = nullptr;
#ifdef Q_OS_WIN
        // 与 sherpa-onnx 用同一个已加载的 dll，避免出现两份 ORT 抢线程池
        HMODULE module = GetModuleHandleW(L"onnxruntime.dll");
        if (!module) module = LoadLibraryW(L"onnxruntime.dll");
        if (!module) {
            LOG_WARN("OrtRuntime: onnxruntime.dll not found");
            return nullptr;
        }
        using GetBaseFn = const OrtApiBase* (ORT_API_CALL*)();
        auto getBase = reinterpret_cast<GetBaseFn>(GetProcAddress(module, "OrtGetApiBase"));
        if (!getBase) return nullptr;
        base = getBase();
#else
        base = OrtGetApiBase();
#endif
        const OrtApi* table = base ? base->GetApi(ORT_API_VERSION) : nullptr;
        if (!table) LOG_WARN("OrtRuntime: runtime older than the headers (API 24 required)");
        return table;
    }();
    return cached;
}

}
