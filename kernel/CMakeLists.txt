project(MTKernel VERSION 1.0.0 LANGUAGES C)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mtkernel/version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mtkernel/version.h
)

add_library(mtkernel SHARED
    src/kernel.c
    src/mt_thread.c
)

target_include_directories(mtkernel
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(mtkernel
    PRIVATE
        MTCAD_KERNEL_BUILD_DLL
    _POSIX_C_SOURCE=200809L
)

set_target_properties(mtkernel PROPERTIES
    OUTPUT_NAME mtkernel
)

# Use libuv for cross-platform threading.
target_link_libraries(mtkernel PRIVATE uv)
