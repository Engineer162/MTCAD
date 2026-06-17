cmake_minimum_required(VERSION 3.25)

project(MTCAD VERSION 0.2.1 LANGUAGES C CXX)

option(MTCAD_BUILD_GUI "Build the desktop ImGui GUI" ON)

include(GNUInstallDirs)

set(MTCAD_PACKAGE_PREFIX "/usr")
set(MTCAD_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
set(MTCAD_INSTALL_DATADIR "${CMAKE_INSTALL_DATADIR}")

configure_file(
    ${CMAKE_SOURCE_DIR}/installer/mtcad-launcher.sh
    ${CMAKE_CURRENT_BINARY_DIR}/mtcad-launcher.sh
    @ONLY
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

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
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(MTCAD_BUILD_GUI)
    install(TARGETS mtcad_gui
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/mtcad
    )

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/gui/assets/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mtcad/assets
    )

    install(FILES ${CMAKE_SOURCE_DIR}/installer/resources/default_imgui.ini
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/mtcad
        RENAME imgui.ini
    )

    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/mtcad-launcher.sh
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME mtcad
    )
endif()

set(CPACK_GENERATOR "DEB;RPM")

set(CPACK_RPM_PACKAGE_REQUIRES
    "libX11, libXext, libXcursor, libXi, libXfixes, libXrandr, libXScrnSaver, vulkan-loader"
)

set(CPACK_PACKAGE_NAME "mtcad")
set(CPACK_PACKAGE_VENDOR "MTCAD")
set(CPACK_PACKAGE_CONTACT "Engineer162")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MTCAD - Multithreaded CAD application")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Engineer162")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Engineering")
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)

include(CPack)