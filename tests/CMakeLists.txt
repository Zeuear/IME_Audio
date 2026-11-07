cmake_minimum_required(VERSION 3.14)

project(ImeAudio_Tests LANGUAGES CXX)

# 测试二进制同样会链接 macOS 平台层的 .mm 实现
if(APPLE)
    enable_language(OBJCXX)
endif()

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Inherit settings from the parent project
include_directories(${CMAKE_SOURCE_DIR}/src)

# Ensure Qt headers are visible for IDE/IntelliSense (Qt6::Core link handles this
# for the compiler, but some IDEs need explicit include dirs)
get_target_property(QT_CORE_INCLUDE_DIRS Qt6::Core INTERFACE_INCLUDE_DIRECTORIES)
if(QT_CORE_INCLUDE_DIRS)
    include_directories(${QT_CORE_INCLUDE_DIRS})
endif()

# ---- unified test binary ----
# All unit tests compiled into a single executable. Each test file keeps its own
# TEST/TestCase names (no collisions today); Add new *.cpp under unit/ and append
# it to SOURCES below. AUTOMOC is on so Q_OBJECT test helpers (e.g. task_queue_async_test)
# are moc'd correctly; it is a no-op for plain test files.
set(CMAKE_AUTOMOC ON)

set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/support/widget_app_environment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/term_replace_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/text_postprocessor_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/extract_progress_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/task_queue_async_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/ollama_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/openai_provider_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/text_polish_service_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/enhancement_fallback_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/translation_service_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/translation_routing_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/translation_model_store_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/language_catalog_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/worker_protocol_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/bergamot_worker_client_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/workflow_manager_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/shortcut_format_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/subtitle_format_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/single_instance_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/live_caption_model_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/ordered_results_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/settings_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/dictation_history_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/snippet_search_index_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/google_sheets_parse_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/dictation_pipeline_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/audio_normalizer_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/transcript_document_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/transcript_store_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/tool_icon_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/control_bar_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/toggle_link_test.cpp
)

# Production translation units needed by the hermetic tests (logic decoupling:
# we link only the seams under test, not the whole app).
set(TEST_DEPS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/snippets/SnippetSearchIndex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/TermsLibraryManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/TextPostProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/audio/AudioNormalizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/ExtractTool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/AppPaths.cpp
    # SingleInstanceGuard 是 Q_OBJECT，.h 也要交给 AUTOMOC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/SingleInstanceGuard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/SingleInstanceGuard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/Logger.cpp
    # TextPolishService is a Q_OBJECT class; include its .cpp + header so AUTOMOC
    # generates the moc code and links it. Logger.cpp is needed (LOG_DEBUG usage).
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/TextPolishService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/TextPolishService.h
    # 本地翻译入口与后端接口；Q_OBJECT 头交给 AUTOMOC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/TranslationService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/TranslationService.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/translation/ITranslationBackend.h
    # 纯语言路由与模型索引解析
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/translation/LanguageRouting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/translation/LanguageRouting.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/installers/RemoteSettingsIndex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/installers/RemoteSettingsIndex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/installers/TranslationModelStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/installers/TranslationModelStore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/translation/BergamotWorkerClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/translation/BergamotWorkerClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/sherpa/LanguageCatalog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/sherpa/LanguageCatalog.h
    # TextPolishService delegates to the provider layer; include those TUs so the
    # hermetic test links (parseOllamaTags is static, but factory/fetchModels need them).
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/textpolish/TextPolishProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/textpolish/TextPolishPrompts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/textpolish/GeminiProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/textpolish/OpenAiProvider.cpp
    # ProcessManager.h holds Q_OBJECT classes (BaseTask / TaskQueueManager /
    # ExtractTask) and must be fed to AUTOMOC so the moc code is generated + linked.
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/ProcessManager.h
    # 平台解耦的系统音频端点控制器（纯 C++，Win 下用 Core Audio COM）
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/WorkflowManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/interfaces/workflow_interfaces.h
    # 神经标点：SherpaPunctuator 封装 + SherpaConfig 的 shouldUseNeuralPunct 判定
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/config/ConfigManager.cpp
    # 字幕内容模型是 Q_OBJECT 类，.h 也要交给 AUTOMOC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/models/LiveCaptionModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/models/LiveCaptionModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/controls/SettingsBinder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/controls/SettingsBinder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/models/DictationHistoryModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/models/DictationHistoryModel.h
    # 实时转录稿子的追加规则，只依赖 QTextDocument
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/models/TranscriptDocument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/models/TranscriptStore.cpp
    # 横条图标：按状态取色并把线条 SVG 渲染成指定颜色，用到 Qt6::Svg
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/controls/ToolIcon.cpp
    # 横条：按工作流状态决定按钮可用性；悬浮窗的位置落点与窗口标志
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/controls/ControlBarState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/controls/FloatingWindow.cpp
    # 横条开关与设置页复选框的双向联动
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/controls/ToggleLink.cpp
    # 话术表格导入：CSV / 表格解析是静态纯函数；xlsx 解压用到 QtCorePrivate 的 QZipReader
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/snippets/GoogleSheetsService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/snippets/GoogleSheetsService.h
)

# 平台层实现按宿主平台选取，与主程序一致（ProcessManager.h 的 moc 代码引用
# runElevatedBlocking，InputInjector 的桩/实现同理，缺一个都会链接失败）。
set(_test_platform_root ${CMAKE_CURRENT_SOURCE_DIR}/../src/platform)
# 必须与 src/CMakeLists.txt 选取同一组文件：RecorderService 按值持有
# SystemAudioEndpointController，漏掉 AudioEndpoint_* 会在链接期报缺符号。
if(WIN32)
    set(_test_platform_dir "win")
    set(_test_platform_ext "cpp")
elseif(APPLE)
    set(_test_platform_dir "mac")
    set(_test_platform_ext "mm")
else()
    set(_test_platform_dir "linux")
    set(_test_platform_ext "cpp")
endif()
foreach(_mod InputInjector AudioEndpoint Elevation Permissions)
    list(APPEND TEST_DEPS
        "${_test_platform_root}/${_test_platform_dir}/${_mod}_${_test_platform_dir}.${_test_platform_ext}")
endforeach()

add_executable(imeaudio_tests ${TEST_SOURCES} ${TEST_DEPS})


if(WIN32)
    target_link_libraries(imeaudio_tests 
        gtest 
        gtest_main 
        Qt6::Core 
        Qt6::Widgets 
        Qt6::Network 
        Qt6::Multimedia 
        Qt6::Concurrent
        Qt6::CorePrivate
        Qt6::Svg
        imm32
    )
elseif(APPLE)
    # 平台层的 .mm 实现所需框架，必须与 src/CMakeLists.txt 保持一致。
    target_link_libraries(imeaudio_tests 
        gtest 
        gtest_main 
        Qt6::Core 
        Qt6::Widgets 
        Qt6::Network 
        Qt6::Multimedia 
        Qt6::Concurrent
        Qt6::CorePrivate
        Qt6::Svg
        "-framework ApplicationServices"
        "-framework AVFoundation"
        "-framework AppKit"
        "-framework CoreAudio"
        "-framework CoreFoundation"
    )
else()
    target_link_libraries(imeaudio_tests 
        gtest 
        gtest_main 
        Qt6::Core 
        Qt6::Widgets 
        Qt6::Network 
        Qt6::Multimedia 
        Qt6::Concurrent
        Qt6::CorePrivate
        Qt6::Svg
    )
endif()

target_compile_definitions(imeaudio_tests PRIVATE IME_AUDIO_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_compile_definitions(imeaudio_tests PRIVATE IME_AUDIO_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../resources")

# Register with CTest (gtest_discover_tests enumerates each TEST case as a CTest entry)
include(GoogleTest)
# 默认的 POST_BUILD 会在链接后立刻运行测试程序来枚举用例；VS 的 RelWithDebInfo 输出目录里没有 Qt DLL，
# 程序启动即 0xc0000135（缺 DLL）导致整个构建失败。改到 ctest 运行时再枚举，构建阶段不启动测试程序。
gtest_discover_tests(imeaudio_tests DISCOVERY_MODE PRE_TEST)
