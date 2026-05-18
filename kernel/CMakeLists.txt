add_library(mtcad_kernel SHARED
    src/kernel.c
    src/mt_thread.c
)

target_include_directories(mtcad_kernel
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(mtcad_kernel
    PRIVATE
        MTCAD_KERNEL_BUILD_DLL
)

set_target_properties(mtcad_kernel PROPERTIES
    OUTPUT_NAME mtcad_kernel
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Use libuv for cross-platform threading, fetch if not present
include(FetchContent)
FetchContent_Declare(
    libuv
    GIT_REPOSITORY https://github.com/libuv/libuv.git
    GIT_TAG v1.44.2
)
FetchContent_MakeAvailable(libuv)

target_link_libraries(mtcad_kernel PRIVATE uv)
