cmake_minimum_required(VERSION 3.15)
project(BrickBreakerHit C)
set(CMAKE_C_STANDARD 99)

find_package(raylib 4.5 REQUIRED)

add_executable(BrickBreakerHit src/main.c)
target_link_libraries(BrickBreakerHit raylib)

if(APPLE)
    target_link_libraries(BrickBreakerHit "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
elseif(UNIX)
    target_link_libraries(BrickBreakerHit m dl pthread)
elseif(WIN32)
    target_link_libraries(BrickBreakerHit winmm)
endif()
