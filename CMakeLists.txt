cmake_minimum_required(VERSION 3.16)
project(maze_project LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8)
endif()

file(GLOB SRC_FILES src/*.cpp)
add_executable(maze_project main.cpp ${SRC_FILES})