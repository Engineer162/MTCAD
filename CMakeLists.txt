cmake_minimum_required(VERSION 3.25)

project(MTCAD VERSION 0.1.0 LANGUAGES C CXX)

option(MTCAD_BUILD_GUI "Build the desktop ImGui GUI" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

add_subdirectory(third_party/SDL)
add_subdirectory(third_party/libuv)

set(MTCAD_IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

add_subdirectory(kernel)

if(MTCAD_BUILD_GUI)
    add_subdirectory(gui)
endif()
