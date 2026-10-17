# FFmpeg 头文件（仅 Windows 使用）

文件转录用 FFmpeg 直接解码音频（`src/audio/AudioFileDecoder`）。`QAudioDecoder` 逐帧经事件循环交付，
1 小时的 m4a 需要约 5 分钟，直接调用 FFmpeg 不到 5 秒。

## 这里有什么

- `include/`：FFmpeg **7.1.1** 的 libavcodec / libavformat / libavutil / libswresample 头文件，
  LGPL-2.1-or-later（见各文件头部声明）。
- `def/`：只列出解码器用到的导出函数。构建时由 MSVC `lib.exe` 生成导入库（见 `src/CMakeLists.txt`），
  仓库里不放任何 `.lib` / `.dll`。

## 运行时用的是谁的 DLL

Qt 6 Multimedia 的 FFmpeg 后端自带 `avcodec-61.dll`、`avformat-61.dll`、`avutil-59.dll`、`swresample-5.dll`
（LGPL 构建），`windeployqt` 会把它们部署到程序目录。本项目只是复用这几个 DLL，不另外分发 FFmpeg。

**不要**把 GPL 版 FFmpeg（如 gyan.dev 的 full build）的 DLL 拷进程序目录或打包。

## 升级 Qt 时

Qt 自带的 FFmpeg 主版本变了（DLL 名里的数字变了），需要同步：

1. 换成对应版本的 FFmpeg 头文件；
2. 改 `def/*.def` 的文件名和 `LIBRARY` 行；
3. 改 `src/CMakeLists.txt` 里的 DLL 名列表。

## macOS / Linux

CMake 在 Qt 的 `lib` 目录（`Qt6_DIR/../..`）里按主版本号找 `libavformat.61.dylib` / `libavformat.so.61` 等四个库，
dylib / so 直接链接，不需要 `.def`。运行时由 `macdeployqt` / linuxdeploy 随 Qt 一起部署。

- 只在 Qt 自己的目录里找，不会链到 Homebrew 或系统包管理器装的 FFmpeg（版本可能不同，也可能是 GPL 构建）。
- 四个库缺任何一个就不启用，配置时打印 `FFmpeg: ... not found`，文件转录回退到 `QAudioDecoder`。
- 尚未在真机上验证：确认配置输出为 `FFmpeg: using Qt bundled libraries`，并检查打包后的 `.app` / AppImage
  里确实带上了这几个库（macOS：`otool -L` 看 `Contents/MacOS/ImeAudio` 的依赖都指向 `@rpath` / `Frameworks`）。
