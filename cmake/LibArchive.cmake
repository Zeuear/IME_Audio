# libarchive：进程内解压 .tar.bz2 / .tar.gz / .zip，取代原先调用系统 `tar` 的做法。
#   target_link_libraries(<t> PRIVATE imeaudio_archive)
#
# 为什么要自带：Windows 自带的 bsdtar 没有内置 bzip2，遇到 .tar.bz2 会再起外部程序 `bzip2 -d`，
# 干净机器上没有它就解压失败；Linux 的 GNU tar 同理。三个平台都靠系统工具是不可控的。
#
# zlib / bzip2 只取源码、自己建静态库，再注入 libarchive：libarchive 自带的 find_package 会去找
# 系统库，找不到会悄悄关掉 bz2/gzip；这里要的是确定地带上，所以绕开探测、直接给宏和链接。
# 其余格式（lzma/zstd/lz4/加密/XML 等）一律关闭，体积和许可证都更干净。

include(FetchContent)

# SOURCE_SUBDIR 指向不存在的目录：只下载解压，不执行对方的 CMakeLists
# URL_HASH：哈希不符就中止配置，防止上游文件被替换或下载损坏后悄悄被编进程序；升级版本时三处一起更新
FetchContent_Declare(imeaudio_zlib_src
    URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
    URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)
FetchContent_Declare(imeaudio_bzip2_src
    URL https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
    URL_HASH SHA256=ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)
FetchContent_Declare(imeaudio_libarchive_src
    URL https://github.com/libarchive/libarchive/releases/download/v3.7.7/libarchive-3.7.7.tar.gz
    URL_HASH SHA256=4cc540a3e9a1eebdefa1045d2e4184831100667e6d7d5b315bb1cbc951f8ddff
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)
FetchContent_MakeAvailable(imeaudio_zlib_src imeaudio_bzip2_src imeaudio_libarchive_src)

set(_zlib_dir  "${imeaudio_zlib_src_SOURCE_DIR}")
set(_bzip2_dir "${imeaudio_bzip2_src_SOURCE_DIR}")
set(_la_src    "${imeaudio_libarchive_src_SOURCE_DIR}")

# ---- zlib（deflate/inflate/crc32 + compress/uncompress：libarchive 在 macOS 的
#      HFS+ 资源分支压缩路径（hfs_write_decmpfs_block/hfs_decompress）会用到
#      compressBound/uncompress，既然已声明 HAVE_ZLIB_H=1 就要把完整 API 面补全，
#      否则只在 macOS 链接时才会炸：Windows/Linux 代码路径不触发这两个符号）----
add_library(imeaudio_zlib STATIC
    "${_zlib_dir}/adler32.c" "${_zlib_dir}/crc32.c" "${_zlib_dir}/deflate.c"
    "${_zlib_dir}/inffast.c" "${_zlib_dir}/inflate.c" "${_zlib_dir}/inftrees.c"
    "${_zlib_dir}/trees.c" "${_zlib_dir}/zutil.c"
    "${_zlib_dir}/compress.c" "${_zlib_dir}/uncompr.c")
target_include_directories(imeaudio_zlib PUBLIC "${_zlib_dir}")
set_target_properties(imeaudio_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---- bzip2 ----
add_library(imeaudio_bzip2 STATIC
    "${_bzip2_dir}/blocksort.c" "${_bzip2_dir}/bzlib.c" "${_bzip2_dir}/compress.c"
    "${_bzip2_dir}/crctable.c" "${_bzip2_dir}/decompress.c" "${_bzip2_dir}/huffman.c"
    "${_bzip2_dir}/randtable.c")
target_include_directories(imeaudio_bzip2 PUBLIC "${_bzip2_dir}")
set_target_properties(imeaudio_bzip2 PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_definitions(imeaudio_bzip2 PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ---- libarchive：只开 bz2/gzip/zip/tar 需要的部分 ----
set(_saved_shared "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
foreach(_opt ENABLE_MBEDTLS ENABLE_NETTLE ENABLE_OPENSSL ENABLE_LIBB2 ENABLE_LZ4 ENABLE_LZO ENABLE_LZMA
             ENABLE_ZSTD ENABLE_ZLIB ENABLE_BZip2 ENABLE_LIBXML2 ENABLE_EXPAT ENABLE_PCREPOSIX
             ENABLE_PCRE2POSIX ENABLE_CNG ENABLE_TAR ENABLE_CPIO ENABLE_CAT
             ENABLE_XATTR ENABLE_ACL ENABLE_ICONV ENABLE_TEST ENABLE_INSTALL)
    set(${_opt} OFF CACHE BOOL "" FORCE)
endforeach()
# libarchive 3.7 的 cmake_minimum_required 写的是 3.2，CMake 4 起直接拒绝，这里降级放行
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory("${_la_src}" "${CMAKE_BINARY_DIR}/libarchive" EXCLUDE_FROM_ALL)
set(BUILD_SHARED_LIBS "${_saved_shared}" CACHE BOOL "" FORCE)

# 绕过 libarchive 自己的 zlib/bzip2 探测：直接告诉它头文件在哪、有这两个库
# （libarchive 对 archive_static 用的是不带关键字的 target_link_libraries，这里必须同一种写法）
target_compile_definitions(archive_static PRIVATE HAVE_ZLIB_H=1 HAVE_BZLIB_H=1)
target_link_libraries(archive_static imeaudio_zlib imeaudio_bzip2)

# 对外只暴露一个目标：头文件 + 静态库宏。三个平台、主程序和测试都链这一个
add_library(imeaudio_archive INTERFACE)
target_link_libraries(imeaudio_archive INTERFACE archive_static)
target_include_directories(imeaudio_archive SYSTEM INTERFACE "${_la_src}/libarchive")
target_compile_definitions(imeaudio_archive INTERFACE LIBARCHIVE_STATIC)
