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
include_directories(${PROJECT_SOURCE_DIR}/src)

# Add the sample test executable
add_executable(sample_test "${CMAKE_CURRENT_SOURCE_DIR}/unit/sample_test.cpp")

# Link with GTest (FetchContent provides GTest::gtest and GTest::gtest_main)
target_link_libraries(sample_test gtest gtest_main)

# Register with CTest
include(GoogleTest)
gtest_discover_tests(sample_test)
