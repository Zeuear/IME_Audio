# ONNX Runtime C API 头文件（v1.24，与 sherpa-onnx 自带的 onnxruntime 运行库同一代）。
# 只提供头文件：Windows 运行时从已加载的 onnxruntime.dll 取 OrtGetApiBase，
# macOS / Linux 由 sherpa_onnx 静态链入的 onnxruntime 提供，所以这里不需要导入库。
#   target_link_libraries(<t> PRIVATE onnxruntime_headers)

add_library(onnxruntime_headers INTERFACE)
target_include_directories(onnxruntime_headers SYSTEM INTERFACE
    "${CMAKE_SOURCE_DIR}/third_party/onnxruntime/include")
