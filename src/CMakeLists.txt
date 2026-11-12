cmake_minimum_required(VERSION 3.14)

set(TARGET_NAME ImeAudio)
configure_file(
    "${CMAKE_SOURCE_DIR}/src/app/version.h.in"
    "${CMAKE_SOURCE_DIR}/src/app/version.h"
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "*.h" "*.hpp")

# platform/ 下每个平台各有一份实现，必须由构建系统挑选，不能被递归 glob 全部吞掉
list(FILTER SOURCES EXCLUDE REGEX "/platform/")
list(FILTER HEADERS EXCLUDE REGEX "/platform/")

# IMEAUDIO_PLATFORM_DIR / _EXT 来自 cmake/Platform.cmake，tests/ 用同一组
file(GLOB PLATFORM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/platform/${IMEAUDIO_PLATFORM_DIR}/*.${IMEAUDIO_PLATFORM_EXT}")
list(APPEND SOURCES ${PLATFORM_SOURCES})
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
# 可执行文件与图标
# ============================================================
if(APPLE)
    # macOS 的 Dock / Finder 图标取自 bundle 内 Resources 下的 .icns，缺了就是白方块
    set(APP_ICON_RESOURCE_MACOS "${CMAKE_SOURCE_DIR}/resources/app_icon.icns")
    set_source_files_properties(${APP_ICON_RESOURCE_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(${TARGET_NAME} MACOSX_BUNDLE ${PROJECT_SOURCES} ${APP_ICON_RESOURCE_MACOS})
elseif(WIN32)
    add_executable(${TARGET_NAME} WIN32 ${PROJECT_SOURCES} "${CMAKE_SOURCE_DIR}/resources/app.rc")
else()
    add_executable(${TARGET_NAME} ${PROJECT_SOURCES})
endif()

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${CMAKE_SOURCE_DIR}/third_party/QHotkey/QHotkey")
target_compile_definitions(${TARGET_NAME} PRIVATE NOMINMAX)


# ============================================================
# 第三方依赖与运行时资源（各自的细节见 cmake/ 下同名文件）
# ============================================================
include(SherpaOnnx)
include(FFmpeg)
include(VadModel)
include(OnnxRuntime)
include(TranslateWorker)

imeaudio_link_ffmpeg(${TARGET_NAME})


# ============================================================
# 翻译文件
# ============================================================
set(TS_FILES ${PROJECT_SOURCE_DIR}/resources/translations/qt_EN.ts ${PROJECT_SOURCE_DIR}/resources/translations/zh_CN.ts)
qt_add_translations(${TARGET_NAME}
    TS_FILES ${TS_FILES}
    LUPDATE_OPTIONS -locations none -no-obsolete # 传给 lupdate 的参数放这里
)


# ============================================================
# 链接
# ============================================================
target_link_libraries(${TARGET_NAME} PRIVATE
    ${QtModules}
    sherpa_onnx
    onnxruntime_headers
    qhotkey
    kissfft
)

if(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE Ole32 Shell32 imm32)
elseif(APPLE)
    target_link_libraries(${TARGET_NAME} PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework CoreServices"
        "-framework CoreFoundation"
        "-framework ApplicationServices"
        "-framework AVFoundation"
        "-framework AppKit"
    )
endif()


# ============================================================
# 构建后：运行库与资源复制到程序旁，最后部署 Qt
# ============================================================
imeaudio_copy_sherpa_runtime(${TARGET_NAME})
imeaudio_deploy_translate_worker(${TARGET_NAME})
imeaudio_deploy_vad_model(${TARGET_NAME})
imeaudio_deploy_qt(${TARGET_NAME})


set_target_properties(${TARGET_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.imeaudio.app
    MACOSX_BUNDLE_BUNDLE_NAME ${TARGET_NAME}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(APPLE)
    # 缺少 NSMicrophoneUsageDescription 时，macOS 会在请求麦克风的瞬间直接 SIGKILL
    # 进程（不是返回拒绝），因此这条是能否跑起来的硬性前提。
    set_target_properties(${TARGET_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../resources/deployment/macos/Info.plist.in"
        MACOSX_BUNDLE_ICON_FILE app_icon.icns
    )
endif()
