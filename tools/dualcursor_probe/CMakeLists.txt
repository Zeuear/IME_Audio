cmake_minimum_required(VERSION 3.16)
project(dualcursor_probe CXX)

# 独立验证原型，不接入主程序构建。
# 构建：cmake -S tools/dualcursor_probe -B build_probe && cmake --build build_probe --config Release
add_executable(probe probe.cpp)
target_compile_features(probe PRIVATE cxx_std_17)
target_compile_definitions(probe PRIVATE UNICODE _UNICODE)
if(MSVC)
    target_compile_options(probe PRIVATE /utf-8 /W4)
endif()
target_link_libraries(probe PRIVATE user32)
