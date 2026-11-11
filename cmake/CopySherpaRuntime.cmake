# 构建后把 CPU 版 sherpa-onnx / onnxruntime DLL 拷到输出目录。
# 用户在"GPU 加速"里装过 CUDA 运行库后，输出目录里的是 GPU 版，此时跳过，避免被 CPU 版覆盖。
if(EXISTS "${DST}/onnxruntime_providers_cuda.dll")
    message(STATUS "GPU runtime present in ${DST}, keep its onnxruntime / sherpa-onnx DLLs")
    return()
endif()
string(REPLACE "," ";" LIBS "${LIBS}")
foreach(_lib IN LISTS LIBS)
    if(EXISTS "${_lib}")
        file(COPY "${_lib}" DESTINATION "${DST}")
    endif()
endforeach()
