# sherpa-onnx 预编译库：按版本自动下载到 third_party/sherpa-onnx，提供接口目标 sherpa_onnx。
#   target_link_libraries(<t> PRIVATE sherpa_onnx)
#   imeaudio_copy_sherpa_runtime(<t>)   # 把运行库复制到可执行文件旁
# 依赖 Platform.cmake（IMEAUDIO_OSX_ARCH）。

set(SHERPA_ONNX_ROOT "${CMAKE_SOURCE_DIR}/third_party/sherpa-onnx")
set(SHERPA_ONNX_VERSION "1.13.4")

# 标记文件:用来判断"是否已经是我们下载并整理好的目录",
# 避免每次 configure 都重新检查/下载,同时避免和"用户自己手动放置的目录"混淆
# Windows 用 CUDA 版运行库（见下），标记带上 -cuda，旧的 CPU 版目录会被识别为过期并重新下载
if(WIN32)
    set(_sherpa_stamp_suffix "-cuda")
endif()
set(SHERPA_ONNX_STAMP_FILE "${SHERPA_ONNX_ROOT}/.sherpa_onnx_version_${SHERPA_ONNX_VERSION}${_sherpa_stamp_suffix}")


if(WIN32)
    set(_sherpa_platform_dir "windows")
    # 统一用 CUDA 版包里的 onnxruntime / sherpa-onnx DLL：它在没有 CUDA 的机器上照样跑 CPU，
    # GPU 安装器只需补 onnxruntime_providers_*（启动时不加载，不会因占用换不掉），CPU / GPU 永远同一版 ORT
    set(_sherpa_archive_name "sherpa-onnx-v${SHERPA_ONNX_VERSION}-cuda-12.x-cudnn-9.x-win-x64-cuda.tar.bz2")

elseif(APPLE)
    set(_sherpa_platform_dir "macos")
    if(IMEAUDIO_OSX_ARCH MATCHES "arm64|aarch64")
        set(_sherpa_archive_name "sherpa-onnx-v${SHERPA_ONNX_VERSION}-osx-arm64-static-no-tts-lib.tar.bz2")
    else()
        # 上游 Intel 资产名用的是 x64，不是 x86_64
        set(_sherpa_archive_name "sherpa-onnx-v${SHERPA_ONNX_VERSION}-osx-x64-static-no-tts-lib.tar.bz2")
    endif()

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_sherpa_platform_dir "linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(_sherpa_archive_name "sherpa-onnx-v${SHERPA_ONNX_VERSION}-linux-x64-static-no-tts-lib.tar.bz2")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(_sherpa_archive_name "sherpa-onnx-v${SHERPA_ONNX_VERSION}-linux-arm64-static-no-tts-lib.tar.bz2")
    else()
        message(FATAL_ERROR "Unsupported Linux architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

else()
    message(FATAL_ERROR "Unsupported platform for auto-downloading sherpa-onnx")
endif()


if(NOT EXISTS "${SHERPA_ONNX_STAMP_FILE}")
    message(STATUS "======================================")
    message(STATUS "sherpa-onnx v${SHERPA_ONNX_VERSION} not found or version mismatch, downloading...")
    message(STATUS "======================================")

    set(_sherpa_url "https://github.com/k2-fsa/sherpa-onnx/releases/download/v${SHERPA_ONNX_VERSION}/${_sherpa_archive_name}")
    set(_sherpa_archive_path "${CMAKE_BINARY_DIR}/_downloads/${_sherpa_archive_name}")
    set(_sherpa_extract_dir "${CMAKE_BINARY_DIR}/_downloads/sherpa-onnx-extracted")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_downloads")

    # 如果之前下载过但没下完整(比如网络中断),重新下载
    if(NOT EXISTS "${_sherpa_archive_path}")
        message(STATUS "Downloading: ${_sherpa_url}")
        file(DOWNLOAD "${_sherpa_url}" "${_sherpa_archive_path}"
             SHOW_PROGRESS
             STATUS _sherpa_download_status
             TLS_VERIFY ON)
        list(GET _sherpa_download_status 0 _sherpa_download_code)
        if(NOT _sherpa_download_code EQUAL 0)
            list(GET _sherpa_download_status 1 _sherpa_download_msg)
            file(REMOVE "${_sherpa_archive_path}") # 下载失败清理掉残缺文件,避免下次误判为"已存在"
            message(FATAL_ERROR "Failed to download sherpa-onnx: ${_sherpa_download_msg}")
        endif()
    else()
        message(STATUS "Using cached archive: ${_sherpa_archive_path}")
    endif()

    # 清理旧的解压目录,避免残留上次失败的中间状态
    if(EXISTS "${_sherpa_extract_dir}")
        file(REMOVE_RECURSE "${_sherpa_extract_dir}")
    endif()
    file(MAKE_DIRECTORY "${_sherpa_extract_dir}")

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xf "${_sherpa_archive_path}"
        WORKING_DIRECTORY "${_sherpa_extract_dir}"
        RESULT_VARIABLE _sherpa_extract_result
    )
    if(NOT _sherpa_extract_result EQUAL 0)
        message(FATAL_ERROR "Failed to extract sherpa-onnx archive at ${_sherpa_archive_path}")
    endif()

    file(GLOB _sherpa_extracted_entries "${_sherpa_extract_dir}/*")
    list(LENGTH _sherpa_extracted_entries _sherpa_entry_count)
    if(_sherpa_entry_count EQUAL 1)
        list(GET _sherpa_extracted_entries 0 _sherpa_inner_root)
    else()
        set(_sherpa_inner_root "${_sherpa_extract_dir}")
    endif()

    if(WIN32)
        set(_sherpa_match_patterns "${_sherpa_inner_root}/lib/*.dll" "${_sherpa_inner_root}/lib/*.lib")
    elseif(APPLE)
        set(_sherpa_match_patterns "${_sherpa_inner_root}/lib/*.a" "${_sherpa_inner_root}/lib/*.dylib")
    else()
        set(_sherpa_match_patterns "${_sherpa_inner_root}/lib/*.a" "${_sherpa_inner_root}/lib/*.so*")
    endif()

    if(NOT EXISTS "${_sherpa_inner_root}/lib")
        message(FATAL_ERROR "Unexpected sherpa-onnx archive structure, expected lib/ under ${_sherpa_inner_root}")
    endif()

    file(REMOVE_RECURSE "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}")
    file(MAKE_DIRECTORY "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}")

    file(GLOB_RECURSE _sherpa_libs ${_sherpa_match_patterns})
    # CUDA provider（约 270 MB）不随程序发布，由"GPU 加速"按需下载
    list(FILTER _sherpa_libs EXCLUDE REGEX "onnxruntime_providers_")
    if(NOT _sherpa_libs)
        message(FATAL_ERROR "No library files found under ${_sherpa_inner_root} matching current platform patterns")
    endif()

    # 拷贝到目标平台的文件夹中
    file(COPY ${_sherpa_libs} DESTINATION "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}")

    file(WRITE "${SHERPA_ONNX_STAMP_FILE}" "sherpa-onnx v${SHERPA_ONNX_VERSION} downloaded and prepared.\n")
    message(STATUS "sherpa-onnx v${SHERPA_ONNX_VERSION} ready at ${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}")
else()
    message(STATUS "sherpa-onnx v${SHERPA_ONNX_VERSION} already present, skip downloading.")
endif()


# ---- 接口目标：头文件、库目录、链接顺序都收在这里 ----
add_library(sherpa_onnx INTERFACE)
target_include_directories(sherpa_onnx INTERFACE "${SHERPA_ONNX_ROOT}/include")
target_link_directories(sherpa_onnx INTERFACE "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}")

if(WIN32)
    target_link_libraries(sherpa_onnx INTERFACE sherpa-onnx-cxx-api sherpa-onnx-c-api)
elseif(APPLE)
    # 静态库按依赖顺序：接口层 → 核心 → 底层算法与模型解析 → 推理后端
    target_link_libraries(sherpa_onnx INTERFACE
        sherpa-onnx-c-api sherpa-onnx-cxx-api
        sherpa-onnx-core
        kaldi-decoder-core kaldi-native-fbank-core sherpa-onnx-kaldifst-core
        sherpa-onnx-fst sherpa-onnx-fstfar ssentencepiece_core kissfft-float
        onnxruntime sherpa-onnx-portaudio_static)
else()
    # 静态库之间互相引用，用 group 让链接器反复扫描
    target_link_libraries(sherpa_onnx INTERFACE
        -Wl,--start-group
        sherpa-onnx-c-api sherpa-onnx-cxx-api sherpa-onnx-core
        kaldi-decoder-core kaldi-native-fbank-core sherpa-onnx-kaldifst-core
        sherpa-onnx-fst sherpa-onnx-fstfar ssentencepiece_core kissfft-float
        onnxruntime sherpa-onnx-portaudio_static
        -Wl,--end-group)
endif()

# 官方预编译的 sherpa-onnx Linux 静态库是用 libstdc++ 旧 ABI 编译的，cxx-api 里凡是把
# std::string 直接摆上函数签名的调用（如 OfflinePunctuation::AddPunctuation）都会因为
# 符号名不匹配导致链接期 undefined reference。这里不整体切换本 target 的 ABI（风险大、
# 且可能牵连 Qt 互操作），而是让 SherpaPunctuator 改走纯 char* 的 C 接口
# （SherpaOnnxCreateOfflinePunctuation / SherpaOfflinePunctuationAddPunct 等），
# 从根源上避开这个 ABI 敏感的调用。

if(WIN32)
    set(_sherpa_runtime_libs
        "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}/sherpa-onnx-cxx-api.dll"
        "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}/sherpa-onnx-c-api.dll"
        "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}/onnxruntime.dll"
    )
elseif(APPLE)
    file(GLOB _sherpa_runtime_libs "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}/*.dylib")
else()
    file(GLOB _sherpa_runtime_libs "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}/*.so*")
endif()

function(imeaudio_copy_sherpa_runtime target)
    set(_copy_commands "")
    foreach(_lib_path IN LISTS _sherpa_runtime_libs)
        if(EXISTS "${_lib_path}")
            list(APPEND _copy_commands COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_lib_path}" "$<TARGET_FILE_DIR:${target}>")
        endif()
    endforeach()
    if(_copy_commands)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMENT "Copying sherpa-onnx runtime libraries to output directory..."
            ${_copy_commands}
        )
    endif()
endfunction()
