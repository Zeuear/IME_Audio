#pragma once

struct OrtApi;

// @brief
// 取得 ONNX Runtime C API 表。运行库与 sherpa-onnx 共用同一份：
// Windows 从 onnxruntime.dll 动态取入口（没有导入库），其它平台由静态链入的 ORT 提供。
// 取不到（库缺失、版本过旧）时返回 nullptr。
namespace OrtRuntime {

const OrtApi* api();

}
