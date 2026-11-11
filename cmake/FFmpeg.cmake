# FFmpeg：文件转录直接解码（见 third_party/ffmpeg/README.md）。
#   imeaudio_link_ffmpeg(<t>)   # 找得到就链接并定义 IMEAUDIO_HAS_FFMPEG，否则回退 QAudioDecoder

set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/third_party/ffmpeg")

function(imeaudio_link_ffmpeg target)
    if(MSVC)
        # Windows：运行时复用 Qt Multimedia 自带、由 windeployqt 部署的 FFmpeg DLL；仓库里只有头文件和 .def，
        # 导入库在构建时用 lib.exe 生成，名字必须与 Qt 自带的 DLL 一致。
        set(_ffmpeg_import_dir "${CMAKE_CURRENT_BINARY_DIR}/ffmpeg")
        set(_ffmpeg_import_libs)
        foreach(_dll avformat-61 avcodec-61 avutil-59 swresample-5)
            set(_def "${FFMPEG_ROOT}/def/${_dll}.def")
            set(_lib "${_ffmpeg_import_dir}/${_dll}.lib")
            add_custom_command(OUTPUT "${_lib}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${_ffmpeg_import_dir}"
                COMMAND "${CMAKE_AR}" /nologo /machine:x64 "/def:${_def}" "/out:${_lib}"
                DEPENDS "${_def}"
                COMMENT "Generating FFmpeg import library ${_dll}.lib"
                VERBATIM)
            list(APPEND _ffmpeg_import_libs "${_lib}")
        endforeach()
        add_custom_target(ffmpeg_import_libs DEPENDS ${_ffmpeg_import_libs})
        add_dependencies(${target} ffmpeg_import_libs)

        target_include_directories(${target} PRIVATE "${FFMPEG_ROOT}/include")
        target_link_libraries(${target} PRIVATE ${_ffmpeg_import_libs})
        target_compile_definitions(${target} PRIVATE IMEAUDIO_HAS_FFMPEG)
    elseif(APPLE OR UNIX)
        # macOS / Linux 的 Qt 把同一套 LGPL FFmpeg 动态库放在 <Qt>/lib，dylib/so 可以直接链接，不需要 .def。
        # 只在 Qt 自己的 lib 目录里找，按主版本号精确匹配，避免链到系统里版本不同（或 GPL 构建）的 FFmpeg；
        # 任何一个找不到就整体不启用，AudioFileDecoder::isAvailable() 返回 false，文件转录回退到 QAudioDecoder。
        get_filename_component(_qt_lib_dir "${Qt6_DIR}/../.." ABSOLUTE)
        set(_ffmpeg_libs)
        set(_ffmpeg_missing)
        foreach(_spec avformat:61 avcodec:61 avutil:59 swresample:5)
            string(REPLACE ":" ";" _parts "${_spec}")
            list(GET _parts 0 _name)
            list(GET _parts 1 _major)
            if(APPLE)
                set(_file "lib${_name}.${_major}.dylib")
            else()
                set(_file "lib${_name}.so.${_major}")
            endif()
            # 用完整路径判断而不是 find_library：后者不认 libavcodec.so.61 这类带版本号的文件名
            if(EXISTS "${_qt_lib_dir}/${_file}")
                list(APPEND _ffmpeg_libs "${_qt_lib_dir}/${_file}")
            else()
                list(APPEND _ffmpeg_missing "${_file}")
            endif()
        endforeach()

        if(_ffmpeg_missing)
            message(STATUS "FFmpeg: ${_ffmpeg_missing} not found in ${_qt_lib_dir}; file transcription uses QAudioDecoder")
        else()
            message(STATUS "FFmpeg: using Qt bundled libraries in ${_qt_lib_dir}")
            target_include_directories(${target} PRIVATE "${FFMPEG_ROOT}/include")
            target_link_libraries(${target} PRIVATE ${_ffmpeg_libs})
            target_compile_definitions(${target} PRIVATE IMEAUDIO_HAS_FFMPEG)
        endif()
    endif()
endfunction()
