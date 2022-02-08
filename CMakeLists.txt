cmake_minimum_required(VERSION 3.10)
project(AppVirtualEnv)

find_package(RapidJSON CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/vendor)
set(imgui_docking_DIR ${VENDOR_DIR}/imgui_docking)
find_package(imgui_docking CONFIG REQUIRED)

set(SRC_FILES 
    src/app.cpp
    src/app_gui.cpp
    src/app_schema.cpp
    src/app_process.cpp
    src/environ.cpp
    src/file_loading.cpp
    src/utils.cpp) 

set(GUI_FILES )

add_executable(main src/main.cpp ${GUI_FILES} ${SRC_FILES})
include_directories(main src/)

set_target_properties(main PROPERTIES CXX_STANDARD 20)
target_link_libraries(main PRIVATE 
    rapidjson fmt::fmt spdlog::spdlog spdlog::spdlog_header_only
    imgui_docking "d3d11.lib" "dxgi.lib" "d3dcompiler.lib")
target_compile_options(main PRIVATE "/MP")
