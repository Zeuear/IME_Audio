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


if(WITH_DEBUG_OUTPUT)
    add_executable(${TARGET_NAME} ${PROJECT_SOURCES})
else()
    add_executable(${TARGET_NAME} WIN32 
                    ${PROJECT_SOURCES} 
                    ${APP_ICON_RESOURCE_WINDOWS})
endif()


# ============================================================
# 自动下载 sherpa-onnx 预编译库
# ============================================================
set(SHERPA_ONNX_ROOT "${CMAKE_SOURCE_DIR}/third_party/sherpa-onnx")
set(SHERPA_ONNX_VERSION "1.13.4")

# 标记文件:用来判断"是否已经是我们下载并整理好的目录",
# 避免每次 configure 都重新检查/下载,同时避免和"用户自己手动放置的目录"混淆
set(SHERPA_ONNX_STAMP_FILE "${SHERPA_ONNX_ROOT}/.sherpa_onnx_version_${SHERPA_ONNX_VERSION}")

if(WIN32 AND NOT EXISTS "${SHERPA_ONNX_STAMP_FILE}")
    message(STATUS "======================================")
    message(STATUS "sherpa-onnx v${SHERPA_ONNX_VERSION} not found or version mismatch, downloading...")
    message(STATUS "======================================")

    set(_sherpa_archive_name "sherpa-onnx-v${SHERPA_ONNX_VERSION}-win-x64-shared-MD-Release-no-tts-lib.tar.bz2")
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

    if(NOT EXISTS "${_sherpa_inner_root}/lib")
        message(FATAL_ERROR "Unexpected sherpa-onnx archive structure, expected lib/ under ${_sherpa_inner_root}")
    endif()

    file(REMOVE_RECURSE "${SHERPA_ONNX_ROOT}/lib/windows")
    file(MAKE_DIRECTORY "${SHERPA_ONNX_ROOT}/lib/windows")

    file(GLOB _sherpa_dlls_libs "${_sherpa_inner_root}/lib/*.dll" "${_sherpa_inner_root}/lib/*.lib")
    if(NOT _sherpa_dlls_libs)
        message(FATAL_ERROR "No dll/lib files found under ${_sherpa_inner_root}/lib")
    endif()
    file(COPY ${_sherpa_dlls_libs} DESTINATION "${SHERPA_ONNX_ROOT}/lib/windows")

    file(WRITE "${SHERPA_ONNX_STAMP_FILE}" "sherpa-onnx v${SHERPA_ONNX_VERSION} downloaded and prepared.\n")
    message(STATUS "sherpa-onnx v${SHERPA_ONNX_VERSION} ready at ${SHERPA_ONNX_ROOT}")
else()
    if(WIN32)
        message(STATUS "sherpa-onnx v${SHERPA_ONNX_VERSION} already present, skip downloading.")
    endif()
endif()


set(SHERPA_CXX_DLL_PATH "${CMAKE_SOURCE_DIR}/third_party/sherpa-onnx/lib/windows/sherpa-onnx-cxx-api.dll")
set(SHERPA_C_DLL_PATH "${CMAKE_SOURCE_DIR}/third_party/sherpa-onnx/lib/windows/sherpa-onnx-c-api.dll")
set(ONNX_DLL_PATH "${CMAKE_SOURCE_DIR}/third_party/sherpa-onnx/lib/windows/onnxruntime.dll")

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SHERPA_CXX_DLL_PATH}" "$<TARGET_FILE_DIR:${TARGET_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SHERPA_CXX_DLL_PATH}" "$<TARGET_FILE_DIR:${TARGET_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ONNX_DLL_PATH}"   "$<TARGET_FILE_DIR:${TARGET_NAME}>"
)


# ============================================================
# QT工具
# ============================================================
set(TS_FILES ${PROJECT_SOURCE_DIR}/resources/translations/qt_EN.ts ${PROJECT_SOURCE_DIR}/resources/translations/zh_CN.ts)
qt_add_translations(${TARGET_NAME}
    TS_FILES ${TS_FILES}
    LUPDATE_OPTIONS -locations none -no-obsolete # 传给 lupdate 的参数放这里
)

qt_add_shaders(${TARGET_NAME} "shaders"
    PREFIX "/shaders"
    BASE "../qml/shaders"   
    FILES
        "../qml/shaders/fluidsphere.frag"
        "../qml/shaders/fluidsphere.vert"
)

set(QHOTKEY_ROOT "${CMAKE_SOURCE_DIR}/third_party/QHotkey")
target_include_directories(${TARGET_NAME} PRIVATE ${QHOTKEY_ROOT}/QHotkey)

set(SHERPA_ONNX_ROOT "${CMAKE_SOURCE_DIR}/third_party/sherpa-onnx")
target_include_directories(${TARGET_NAME} PRIVATE ${SHERPA_ONNX_ROOT}/include)
target_link_directories(${TARGET_NAME} PRIVATE ${SHERPA_ONNX_ROOT}/lib/windows)


target_compile_definitions(${TARGET_NAME}
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)


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
else()
    target_link_libraries(${TARGET_NAME} 
        ${QtModules}
        sherpa-onnx-cxx-api    
        sherpa-onnx-c-api 
        qhotkey
        kissfft
    )
endif()


set_target_properties(${TARGET_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)