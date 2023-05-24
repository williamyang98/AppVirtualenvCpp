cmake_minimum_required(VERSION 3.10)
project(AppVirtualEnv)

find_package(RapidJSON CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(imgui REQUIRED)

set(SRC_FILES 
    src/app.cpp
    src/app_gui.cpp
    src/app_schema.cpp
    src/app_process.cpp
    src/managed_config.cpp
    src/scrolling_buffer.cpp
    src/environ.cpp
    src/file_loading.cpp
    src/utils.cpp) 

add_executable(main src/main.cpp ${SRC_FILES})
include_directories(main src/)

set_target_properties(main PROPERTIES CXX_STANDARD 20)
target_link_libraries(main PRIVATE 
    imgui
    rapidjson fmt::fmt spdlog::spdlog spdlog::spdlog_header_only)
target_compile_options(main PRIVATE "/MP")

add_executable(print_environment src/print_environment.c)