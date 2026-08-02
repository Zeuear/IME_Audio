cmake_minimum_required(VERSION 3.14)

project(ImeAudio VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/app)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/app/lib/debug/${platform})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/app/lib/debug/${platform})
else()
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/app/lib/release/${platform})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/app/lib/release/${platform})
endif()

# Win32 unicode fix and msvc utf8 flag
if(WIN32)
    add_definitions(-DUNICODE)
    add_definitions(-D_UNICODE)

    if(MSVC)
        add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
        add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    endif()
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WITH_DEBUG_OUTPUT "Enable debug output" OFF)

find_package(QT NAMES Qt6 COMPONENTS 
            Core 
            Widgets 
            Network 
            Concurrent REQUIRED)

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS 
            Core 
            Widgets 
            Network 
            Multimedia 
            Concurrent 
            Qml 
            Quick 
            QuickWidgets 
            LinguistTools 
            ShaderTools REQUIRED)

set(QtModules 
      Qt${QT_VERSION_MAJOR}::Core 
      Qt${QT_VERSION_MAJOR}::Widgets 
      Qt${QT_VERSION_MAJOR}::Network 
      Qt${QT_VERSION_MAJOR}::Concurrent 
      Qt${QT_VERSION_MAJOR}::Multimedia  
      Qt${QT_VERSION_MAJOR}::Qml 
      Qt${QT_VERSION_MAJOR}::Quick 
      Qt${QT_VERSION_MAJOR}::QuickWidgets 
      Qt${QT_VERSION_MAJOR}::ShaderTools 
)

# 3rdparty
add_subdirectory(third_party/kissfft)
add_subdirectory(third_party/QHotkey)

# Testing setup
enable_testing()

# src
add_subdirectory(src)

# tests
add_subdirectory(tests)
