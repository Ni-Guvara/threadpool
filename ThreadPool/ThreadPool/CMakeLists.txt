cmake_minimum_required(VERSION 3.0)					# CMAKE最小需要版本

project(main)										# 项目名称

set(CMAKE_CXX_STANDARD 17)						    # CXX标准

add_subdirectory(threadpool)					    # 编译子目录

add_executable(main main.cpp)
target_link_libraries(main PRIVATE threadpool)      # 链接生成的库