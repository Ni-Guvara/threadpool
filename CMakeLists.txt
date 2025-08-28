cmake_minimum_required(VERSION 3.2)

project(threadpool)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_CFLAGS} -g)


include_directories(${CMAKE_SOURCE_DIR}/include)
link_directories(${CMAKE_SOURCE_DIR}/lib)

add_subdirectory(src)
add_subdirectory(example)