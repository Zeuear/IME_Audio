# 平台选择：src/ 与 tests/ 必须挑同一组 platform/ 实现，统一在这里定义。
#   IMEAUDIO_PLATFORM_DIR / IMEAUDIO_PLATFORM_EXT：src/platform/<dir>/*.<ext>
#   IMEAUDIO_OSX_ARCH：macOS 目标架构（sherpa-onnx、翻译 worker 都按它选预编译包）

if(WIN32)
    set(IMEAUDIO_PLATFORM_DIR "win")
    set(IMEAUDIO_PLATFORM_EXT "cpp")
elseif(APPLE)
    set(IMEAUDIO_PLATFORM_DIR "mac")
    set(IMEAUDIO_PLATFORM_EXT "mm")
else()
    set(IMEAUDIO_PLATFORM_DIR "linux")
    set(IMEAUDIO_PLATFORM_EXT "cpp")
endif()

if(APPLE)
    # 指定了 CMAKE_OSX_ARCHITECTURES（如在 Apple Silicon 上交叉编 x86_64）时以它为准，否则看构建机 CPU。
    # sherpa 是单架构静态库，不支持一次编通用包
    if(CMAKE_OSX_ARCHITECTURES)
        set(IMEAUDIO_OSX_ARCH "${CMAKE_OSX_ARCHITECTURES}")
    else()
        set(IMEAUDIO_OSX_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    if(IMEAUDIO_OSX_ARCH MATCHES ";")
        message(FATAL_ERROR "sherpa-onnx static libs are single-arch; build arm64 and x86_64 separately")
    endif()
endif()
