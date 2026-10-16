# 把 Qt 运行时依赖（DLL / 插件 / qml 模块）部署到可执行文件旁，让构建产物开箱即用。
# Linux 打包由 CI 的 linuxdeploy 负责，这里不处理。

option(IMEAUDIO_DEPLOY_QT "Windows: run windeployqt after building ImeAudio" ON)

# 优先用 Qt 导出的工具 target，缺失时再按 Qt 标准目录布局（<prefix>/lib/cmake/Qt6 → <prefix>/bin）查找
function(_imeaudio_find_qt_tool out_var tool_name)
    set(_target "Qt6::${tool_name}")
    if(TARGET ${_target})
        get_target_property(_path ${_target} IMPORTED_LOCATION)
    endif()
    if(NOT _path)
        find_program(_path ${tool_name} HINTS "${Qt6_DIR}/../../../bin")
    endif()
    set(${out_var} "${_path}" PARENT_SCOPE)
endfunction()

# imeaudio_deploy_qt(<target> [QMLDIR <dir>])
#   Windows：构建后自动执行 windeployqt（与 CI 打包使用同一组参数）
#   macOS  ：提供 deploy_macos 目标（不在默认构建内），见下方说明
function(imeaudio_deploy_qt target)
    cmake_parse_arguments(ARG "" "QMLDIR" "" ${ARGN})

    if(WIN32 AND IMEAUDIO_DEPLOY_QT)
        _imeaudio_find_qt_tool(_windeployqt windeployqt)
        if(NOT _windeployqt)
            message(WARNING "windeployqt not found, skip Qt deployment for ${target}")
            return()
        endif()

        set(_qml_args "")
        if(ARG_QMLDIR)
            # 不带 --qmldir 时 windeployqt 只拷 DLL，不会带上 QtQuick 的 qml 模块
            set(_qml_args --qmldir "${ARG_QMLDIR}")
        endif()

        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${_windeployqt}"
                $<IF:$<CONFIG:Debug>,--debug,--release>
                $<$<NOT:$<CONFIG:Debug>>:--compiler-runtime>
                ${_qml_args}
                "$<TARGET_FILE:${target}>"
            COMMENT "Deploying Qt runtime for ${target} (windeployqt)..."
            VERBATIM
        )
    elseif(APPLE)
        _imeaudio_find_qt_tool(_macdeployqt macdeployqt)
        if(NOT _macdeployqt)
            message(WARNING "macdeployqt not found, deploy_macos target is unavailable")
            return()
        endif()

        set(_qml_args "")
        if(ARG_QMLDIR)
            set(_qml_args "-qmldir=${ARG_QMLDIR}")
        endif()

        # 不挂到 POST_BUILD：macdeployqt 会改写二进制使链接器签名失效，每次开发构建都跑既慢又会让
        # TCC 授权对不上；发布时按需执行 `cmake --build build --target deploy_macos`，
        # 之后仍须重新 codesign（见 release.yml）。
        add_custom_target(deploy_macos
            COMMAND "${_macdeployqt}" "$<TARGET_BUNDLE_DIR:${target}>" ${_qml_args}
            DEPENDS ${target}
            COMMENT "Deploying Qt frameworks into ${target}.app (macdeployqt)..."
            VERBATIM
        )
    endif()
endfunction()
