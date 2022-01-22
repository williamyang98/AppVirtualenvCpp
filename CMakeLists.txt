cmake_minimum_required(VERSION 3.10)
project(AppVirtualEnv)


find_package(RapidJSON CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set(SRC_FILES 
    src/app.cpp
    src/app_gui.cpp
    src/app_schema.cpp
    src/app_process.cpp
    src/environ.cpp
    src/file_loading.cpp
    src/utils.cpp
) 

set(GUI_FILES )

add_executable(main src/main.cpp ${GUI_FILES} ${SRC_FILES})
include_directories(main src/)

set_target_properties(main PROPERTIES CXX_STANDARD 20)
target_link_libraries(main PRIVATE rapidjson fmt::fmt imgui::imgui spdlog::spdlog spdlog::spdlog_header_only opengl32)
target_compile_options(main PRIVATE "/MP")