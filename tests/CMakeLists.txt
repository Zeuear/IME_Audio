cmake_minimum_required(VERSION 3.14)

project(ImeAudio_Tests LANGUAGES CXX)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/term_replace_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/text_postprocessor_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/extract_progress_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/task_queue_async_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/ollama_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/openai_provider_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/workflow_manager_test.cpp
)

# Production translation units needed by the hermetic tests (logic decoupling:
# we link only the seams under test, not the whole app).
set(TEST_DEPS
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TermsLibraryManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TextPostProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/ExtractTool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/Logger.cpp
    # TextPolishService is a Q_OBJECT class; include its .cpp + header so AUTOMOC
    # generates the moc code and links it. Logger.cpp is needed (LOG_DEBUG usage).
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TextPolishService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TextPolishService.h
    # TextPolishService delegates to the provider layer; include those TUs so the
    # hermetic test links (parseOllamaTags is static, but factory/fetchModels need them).
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/textpolish/TextPolishProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/textpolish/TextPolishPrompts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/textpolish/GeminiProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/textpolish/OpenAiProvider.cpp
    # ProcessManager.h holds Q_OBJECT classes (BaseTask / TaskQueueManager /
    # ExtractTask) and must be fed to AUTOMOC so the moc code is generated + linked.
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/ProcessManager.h
    # 平台解耦的系统音频端点控制器（纯 C++，Win 下用 Core Audio COM）
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/WorkflowManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/InputInjector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/interfaces/workflow_interfaces.h
    # 神经标点：SherpaPunctuator 封装 + SherpaConfig 的 shouldUseNeuralPunct 判定
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ConfigManager.cpp
)

add_executable(imeaudio_tests ${TEST_SOURCES} ${TEST_DEPS})
target_include_directories(imeaudio_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/sherpa-onnx/include)
target_link_libraries(imeaudio_tests gtest gtest_main Qt6::Core Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::Concurrent imm32)

# Register with CTest (gtest_discover_tests enumerates each TEST case as a CTest entry)
include(GoogleTest)
gtest_discover_tests(imeaudio_tests)
