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
set(MINIAUDIO_ROOT "${CMAKE_SOURCE_DIR}/third_party/miniaudio")
set(MINIAUDIO_CPP_ROOT "${CMAKE_SOURCE_DIR}/third_party/miniaudio/miniaudio.c")


if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_SOURCE_DIR}/resources/app.rc")
endif()

if(WITH_DEBUG_OUTPUT)
    add_executable(${TARGET_NAME} ${PROJECT_SOURCES})
else()
    add_executable(${TARGET_NAME} WIN32 
                    ${PROJECT_SOURCES} 
                    ${MINIAUDIO_CPP_ROOT}
                    ${APP_ICON_RESOURCE_WINDOWS})
endif()

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

target_include_directories(${TARGET_NAME} PRIVATE ${MINIAUDIO_ROOT})

set(QHOTKEY_ROOT "${CMAKE_SOURCE_DIR}/third_party/QHotkey")
target_include_directories(${TARGET_NAME} PRIVATE ${QHOTKEY_ROOT}/QHotkey)

set(SHERPA_ONNX_ROOT "${CMAKE_SOURCE_DIR}/third_party/sherpa-onnx")
target_include_directories(${TARGET_NAME} PRIVATE ${SHERPA_ONNX_ROOT}/include)
target_link_directories(${TARGET_NAME} PRIVATE ${SHERPA_ONNX_ROOT}/lib/windows)


target_compile_definitions(${TARGET_NAME}
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)
target_link_libraries(${TARGET_NAME} 
    ${QtModules}
    sherpa-onnx-cxx-api    
    sherpa-onnx-c-api 
    qhotkey
    kissfft
)

set_target_properties(${TARGET_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)