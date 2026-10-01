cmake_minimum_required(VERSION 3.16)
project(maze_project LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8)
endif()

# ---- Qt 相关配置 ----
set(CMAKE_AUTOMOC ON)   # 自动处理Qt的信号槽机制(Q_OBJECT宏)
set(CMAKE_AUTOUIC ON)   # 自动处理.ui界面文件(以后用Qt Designer画界面会用到)
set(CMAKE_AUTORCC ON)   # 自动处理资源文件

set(CMAKE_PREFIX_PATH "D:/Qt/6.12.0/msvc2022_64")  # 改成你实际的Qt路径
find_package(Qt6 REQUIRED COMPONENTS Widgets)
# ---------------------

file(GLOB SRC_FILES src/*.cpp)
add_executable(maze_project main.cpp ${SRC_FILES})

target_link_libraries(maze_project PRIVATE Qt6::Widgets)