file(GLOB SOURCES "*.cpp")

add_library(threadpool SHARED ${SOURCES})

install(TARGETS threadpool 
    LIBRARY DESTINATION lib)

install(DIRECTORY ../include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

