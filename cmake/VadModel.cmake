# silero_vad.onnx：configure 时下载到构建目录，构建后复制到程序旁。
#   imeaudio_deploy_vad_model(<t>)

set(VAD_MODEL_URL "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx")
set(VAD_MODEL_DIR "${CMAKE_BINARY_DIR}/sherpa/vad")
set(VAD_MODEL_PATH "${VAD_MODEL_DIR}/silero_vad.onnx")

# 官方发布的 silero_vad.onnx 的 SHA256（建议自己算一次填进去，防止下载损坏或被篡改）
set(VAD_MODEL_SHA256 "" CACHE STRING "Expected SHA256 of silero_vad.onnx")

if(NOT EXISTS "${VAD_MODEL_PATH}")
    message(STATUS "Downloading silero_vad.onnx ...")
    file(MAKE_DIRECTORY "${VAD_MODEL_DIR}")

    if(VAD_MODEL_SHA256)
        file(DOWNLOAD "${VAD_MODEL_URL}" "${VAD_MODEL_PATH}"
            EXPECTED_HASH SHA256=${VAD_MODEL_SHA256}
            SHOW_PROGRESS
            TLS_VERIFY ON)
    else()
        file(DOWNLOAD "${VAD_MODEL_URL}" "${VAD_MODEL_PATH}"
            SHOW_PROGRESS
            TLS_VERIFY ON)
    endif()

    if(NOT EXISTS "${VAD_MODEL_PATH}")
        message(FATAL_ERROR "Failed to download silero_vad.onnx from ${VAD_MODEL_URL}")
    endif()
else()
    message(STATUS "silero_vad.onnx already exists, skip downloading.")
endif()

function(imeaudio_deploy_vad_model target)
    # 落点必须与 AppPaths::vadModelFile() 的解析结果一致：macOS 上是 bundle 的
    # Contents/Resources，其它平台是可执行文件同级目录。
    if(APPLE)
        set(_vad_dest_dir "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/sherpa/vad")
    else()
        set(_vad_dest_dir "$<TARGET_FILE_DIR:${target}>/sherpa/vad")
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_vad_dest_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VAD_MODEL_PATH}"
            "${_vad_dest_dir}/silero_vad.onnx"
        COMMENT "Copying silero_vad.onnx to output directory"
    )
endfunction()
