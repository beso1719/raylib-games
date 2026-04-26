cmake_minimum_required(VERSION 3.15)
project(BrickBreaker C)
set(CMAKE_C_STANDARD 99)

find_package(raylib 4.5 REQUIRED)

add_executable(BrickBreaker src/main.c)
target_link_libraries(BrickBreaker raylib)

if(APPLE)
    target_link_libraries(BrickBreaker
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
elseif(UNIX)
    target_link_libraries(BrickBreaker m dl pthread)
elseif(WIN32)
    target_link_libraries(BrickBreaker winmm)
endif()
