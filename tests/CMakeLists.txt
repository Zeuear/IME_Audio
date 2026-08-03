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

# ---- sample smoke test ----
add_executable(sample_test "${CMAKE_CURRENT_SOURCE_DIR}/unit/sample_test.cpp")
target_link_libraries(sample_test gtest gtest_main)

# ---- term-replace regression tests ----
# Pure-logic tests: link against the TermsLibraryManager translation unit
# without requiring Qt widgets or network.
add_executable(term_replace_test "${CMAKE_CURRENT_SOURCE_DIR}/unit/term_replace_test.cpp")
target_link_libraries(term_replace_test gtest gtest_main Qt6::Core)
target_sources(term_replace_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/TermsLibraryManager.cpp)

# ---- text post-processor tests ----
add_executable(text_postprocessor_test "${CMAKE_CURRENT_SOURCE_DIR}/unit/text_postprocessor_test.cpp")
target_link_libraries(text_postprocessor_test gtest gtest_main Qt6::Core)
target_sources(text_postprocessor_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TextPostProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/TermsLibraryManager.cpp
)

# Register all tests with CTest
include(GoogleTest)
gtest_discover_tests(sample_test)
gtest_discover_tests(term_replace_test)
gtest_discover_tests(text_postprocessor_test)
