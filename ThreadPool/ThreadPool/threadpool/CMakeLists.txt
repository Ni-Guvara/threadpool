project(threadpool)

add_library(threadpool "src/threadpool.cpp" "include/test.h")
target_include_directories(threadpool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

