file(GLOB SOURCES "*.cpp")

add_executable(test  ${SOURCES})
target_link_libraries(test threadpool pthread)

install(TARGETS test 
    RUNTIME DESTINATION bin)