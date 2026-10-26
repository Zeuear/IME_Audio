# 本目录不是独立工程：它由 bergamot-translator 的源码树通过 add_subdirectory 引入（见 build_win.sh），
# 这样才能直接链接 bergamot-translator 及其静态依赖（OpenBLAS、protobuf、pcre2）。
add_executable(translate_worker translate_worker.cpp)
target_link_libraries(translate_worker PRIVATE bergamot-translator)
