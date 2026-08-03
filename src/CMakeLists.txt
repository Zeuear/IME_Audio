cmake_minimum_required(VERSION 3.14)

set(TARGET_NAME ImeAudio)
configure_file(
    "${CMAKE_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_SOURCE_DIR}/src/version.h"
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.cpp" "utils/*.cpp" "sherpa/*.cpp" "widget/*.cpp" "cuda/*.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "*.h" "*.hpp" "utils/*.h" "sherpa/*.h" "widget/*.h" "cuda/*.h")
file(GLOB_RECURSE UIFILES CONFIGURE_DEPENDS "*.ui")
file(GLOB_RECURSE QRCFILES CONFIGURE_DEPENDS "*.qrc")
set(PROJECT_SOURCES
    ${HEADERS}
    ${SOURCES}
    ${UIFILES}
    ${QRCFILES}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PROJECT_SOURCES} )


# ============================================================
# 设置Windows图标
# ============================================================
if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_SOURCE_DIR}/resources/app.rc")
endif()


if(APPLE)
    add_executable(${TARGET_NAME} MACOSX_BUNDLE
                    ${PROJECT_SOURCES})
elseif(WIN32)
    add_executable(${TARGET_NAME} WIN32 
                    ${PROJECT_SOURCES} 
                    ${APP_ICON_RESOURCE_WINDOWS})
else()
    add_executable(${TARGET_NAME}
                    ${PROJECT_SOURCES})
endif()


# ============================================================
# 自动下载 sherpa-onnx 预编译库
# ============================================================
set(SHERPA_ONNX_ROOT "${CMAKE_SOURCE_DIR}/third_party/sherpa-onnx")
set(SHERPA_ONNX_VERSION "1.13.4")

# 标记文件:用来判断"是否已经是我们下载并整理好的目录",
# 避免每次 configure 都重新检查/下载,同时避免和"用户自己手动放置的目录"混淆
set(SHERPA_ONNX_STAMP_FILE "${SHERPA_ONNX_ROOT}/.sherpa_onnx_version_${SHERPA_ONNX_VERSION}")


if(WIN32)
    set(_sherpa_platform_dir "windows")
    set(_sherpa_archive_name "sherpa-onnx-v${SHERPA_ONNX_VERSION}-win-x64-shared-MD-Release-no-tts-lib.tar.bz2")

elseif(APPLE)
    set(_sherpa_platform_dir "macos")
    # 自动识别 macOS 是 Apple Silicon (arm64) 还是 Intel (x86_64)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(_sherpa_archive_name "sherpa-onnx-v${SHERPA_ONNX_VERSION}-osx-arm64-static-no-tts-lib.tar.bz2")
    else()
        set(_sherpa_archive_name "sherpa-onnx-v${SHERPA_ONNX_VERSION}-osx-x86_64-static-no-tts-lib.tar.bz2")
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
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_sherpa_match_patterns "${_sherpa_inner_root}/lib/*.a" "${_sherpa_inner_root}/lib/*.so*")
    else()
        message(FATAL_ERROR "Unsupported platform for auto-downloading sherpa-onnx")
    endif()


    if(NOT EXISTS "${_sherpa_inner_root}/lib")
        message(FATAL_ERROR "Unexpected sherpa-onnx archive structure, expected lib/ under ${_sherpa_inner_root}")
    endif()

    file(REMOVE_RECURSE "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}")
    file(MAKE_DIRECTORY "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}")
    
    file(GLOB_RECURSE _sherpa_libs CONFIGURE_DEPENDS ${_sherpa_match_patterns})
    if(NOT _sherpa_libs)
        message(FATAL_ERROR "No library files found under ${_sherpa_inner_root} matching current platform patterns")
    endif()

    # 拷贝到目标平台的文件夹中
    file(COPY ${_sherpa_libs} DESTINATION "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}")
    
    file(WRITE "${SHERPA_ONNX_STAMP_FILE}" "sherpa-onnx v${SHERPA_ONNX_VERSION} downloaded and prepared.\n")
    message(STATUS "sherpa-onnx v${SHERPA_ONNX_VERSION} ready at ${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}")
else()
    if(WIN32)
        message(STATUS "sherpa-onnx v${SHERPA_ONNX_VERSION} already present, skip downloading.")
    endif()
endif()


if(WIN32)
    set(_sherpa_runtime_libs
        "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}/sherpa-onnx-cxx-api.dll"
        "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}/sherpa-onnx-c-api.dll"
        "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}/onnxruntime.dll"
    )
elseif(APPLE)
    file(GLOB _sherpa_runtime_libs "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}/*.dylib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    file(GLOB _sherpa_runtime_libs "${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir}/*.so*")
endif()

if(_sherpa_runtime_libs)
    set(_copy_commands "")
    foreach(_lib_path IN LISTS _sherpa_runtime_libs)
        if(EXISTS "${_lib_path}")
            list(APPEND _copy_commands COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_lib_path}" "$<TARGET_FILE_DIR:${TARGET_NAME}>")
        endif()
    endforeach()

    if(_copy_commands)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMENT "Copying sherpa-onnx runtime libraries to output directory..."
            ${_copy_commands}
        )
    endif()
endif()

set(SHERPA_ONNX_ROOT "${CMAKE_SOURCE_DIR}/third_party/sherpa-onnx")
target_include_directories(${TARGET_NAME} PRIVATE ${SHERPA_ONNX_ROOT}/include)
target_link_directories(${TARGET_NAME} PRIVATE ${SHERPA_ONNX_ROOT}/lib/${_sherpa_platform_dir})


# ============================================================
# QT工具
# ============================================================
set(TS_FILES ${PROJECT_SOURCE_DIR}/resources/translations/qt_EN.ts ${PROJECT_SOURCE_DIR}/resources/translations/zh_CN.ts)
qt_add_translations(${TARGET_NAME}
    TS_FILES ${TS_FILES}
    LUPDATE_OPTIONS -locations none -no-obsolete # 传给 lupdate 的参数放这里
)

set(QHOTKEY_ROOT "${CMAKE_SOURCE_DIR}/third_party/QHotkey")
target_include_directories(${TARGET_NAME} PRIVATE ${QHOTKEY_ROOT}/QHotkey)

target_compile_definitions(${TARGET_NAME}
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)



# ============================================================
# VAD Model Download
# ============================================================
set(VAD_MODEL_URL "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx")
set(VAD_MODEL_DIR "${CMAKE_BINARY_DIR}/sherpa/vad")
set(VAD_MODEL_PATH "${VAD_MODEL_DIR}/silero_vad.onnx")

# 官方发布的 silero_vad.onnx 的 SHA256（建议自己算一次填进去，防止下载损坏或被篡改）
set(VAD_MODEL_SHA256 "" CACHE STRING "Expected SHA256 of silero_vad.onnx")

if(NOT EXISTS "${VAD_MODEL_PATH}")
    message(STATUS "Downloading silero_vad.onnx ...")
    file(MAKE_DIRECTORY "${VAD_MODEL_DIR}")

    if(VAD_MODEL_SHA256)
        file(DOWNLOAD
            "${VAD_MODEL_URL}"
            "${VAD_MODEL_PATH}"
            EXPECTED_HASH SHA256=${VAD_MODEL_SHA256}
            SHOW_PROGRESS
            TLS_VERIFY ON
        )
    else()
        file(DOWNLOAD
            "${VAD_MODEL_URL}"
            "${VAD_MODEL_PATH}"
            SHOW_PROGRESS
            TLS_VERIFY ON
        )
    endif()

    if(NOT EXISTS "${VAD_MODEL_PATH}")
        message(FATAL_ERROR "Failed to download silero_vad.onnx from ${VAD_MODEL_URL}")
    endif()
else()
    message(STATUS "silero_vad.onnx already exists, skip downloading.")
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/sherpa/vad"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VAD_MODEL_PATH}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/sherpa/vad/silero_vad.onnx"
    COMMENT "Copying silero_vad.onnx to output directory"
)



if(WIN32)
    target_link_libraries(ImeAudio PRIVATE
        ${QtModules}
        sherpa-onnx-cxx-api    
        sherpa-onnx-c-api 
        qhotkey
        kissfft
        Ole32
        Shell32
    )
elseif(APPLE)
    target_link_libraries(${TARGET_NAME} 
        ${QtModules}
        # 1. 接口层（先链接）
        sherpa-onnx-c-api
        sherpa-onnx-cxx-api
        # 2. 核心业务层
        sherpa-onnx-core
        # 3. 各种底层算法与模型解析依赖库（后链接）
        kaldi-decoder-core
        kaldi-native-fbank-core
        sherpa-onnx-kaldifst-core
        sherpa-onnx-fst
        sherpa-onnx-fstfar
        ssentencepiece_core
        kissfft-float
        # 4. 最底层的硬件/推理后端
        onnxruntime
        sherpa-onnx-portaudio_static
        qhotkey
        kissfft
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework CoreServices"
        "-framework CoreFoundation"
    )
    
    # target_link_options(${TARGET_NAME} PRIVATE "-Wl,-all_load")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TARGET_NAME} PRIVATE
        -Wl,--start-group
        ${QtModules}
        sherpa-onnx-c-api sherpa-onnx-cxx-api sherpa-onnx-core
        kaldi-decoder-core kaldi-native-fbank-core sherpa-onnx-kaldifst-core
        sherpa-onnx-fst sherpa-onnx-fstfar ssentencepiece_core kissfft-float
        onnxruntime sherpa-onnx-portaudio_static
        qhotkey
        kissfft
        -Wl,--end-group
    )
endif()


set_target_properties(${TARGET_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)