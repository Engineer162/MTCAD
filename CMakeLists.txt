cmake_minimum_required(VERSION 3.25)

project(MTCAD VERSION 0.1.0 LANGUAGES C CXX)

option(MTCAD_BUILD_GUI "Build the desktop ImGui GUI" ON)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

# Build SDL3 as static library for self-contained packaging
set(SDL_SHARED OFF CACHE BOOL "Build shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build static library" FORCE)
add_subdirectory(third_party/SDL)
add_subdirectory(third_party/libuv)

set(MTCAD_IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

add_subdirectory(kernel)

if(MTCAD_BUILD_GUI)
    add_subdirectory(gui)
endif()

install(TARGETS mtcad_kernel
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/mtcad
    NAMELINK_SKIP
)

if(MTCAD_BUILD_GUI)
    install(TARGETS mtcad_gui
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/mtcad
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/gui/assets/
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/mtcad/assets
    )

    install(FILES ${CMAKE_SOURCE_DIR}/installer/resources/default_imgui.ini
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/mtcad
        RENAME imgui.ini
    )

    install(PROGRAMS ${CMAKE_SOURCE_DIR}/installer/mtcad-launcher.sh
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME mtcad
    )
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "mtcad")
set(CPACK_PACKAGE_VENDOR "MTCAD")
set(CPACK_PACKAGE_CONTACT "Engineer162")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MTCAD is a multithreaded CAD program")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Engineer162")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/installer")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
include(CPack)
