find_package(Vulkan REQUIRED)

set(MTCAD_SDL3_TARGET SDL3::SDL3)

add_library(mtcad_imgui STATIC
    ${MTCAD_IMGUI_DIR}/imgui.cpp
    ${MTCAD_IMGUI_DIR}/imgui_draw.cpp
    ${MTCAD_IMGUI_DIR}/imgui_tables.cpp
    ${MTCAD_IMGUI_DIR}/imgui_widgets.cpp
    ${MTCAD_IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${MTCAD_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
)

target_include_directories(mtcad_imgui
    PUBLIC
        ${MTCAD_IMGUI_DIR}
        ${MTCAD_IMGUI_DIR}/backends
)

target_link_libraries(mtcad_imgui
    PUBLIC
        Vulkan::Vulkan
        ${MTCAD_SDL3_TARGET}
)

add_executable(mtcad_gui
    src/icon_loader.cpp
    src/main.cpp
    src/theme_manager.cpp
    src/tools/tool_manager.cpp
    src/tools/tools_init.cpp
    src/tools/sketch/create/line_tool.cpp
    src/tools/sketch/create/rectangle_tool.cpp
    src/tools/sketch/create/circle_tool.cpp
    src/windows/settings_window.cpp
    src/windows/extrude_window.cpp
    src/windows/tool_window.cpp
    src/windows/toolbar_window.cpp
    src/windows/viewport_window.cpp
    src/windows/workspace_browser_window.cpp
)

target_compile_definitions(mtcad_gui
    PRIVATE
        MTCAD_KERNEL_USE_DLL
)

target_include_directories(mtcad_gui
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/stb
)

target_link_libraries(mtcad_gui
    PRIVATE
        mtcad_kernel
        mtcad_imgui
        Vulkan::Vulkan
        ${MTCAD_SDL3_TARGET}
)

set_target_properties(mtcad_gui PROPERTIES
    OUTPUT_NAME mtcad_gui
)

if(WIN32)
    set(MTCAD_RELEASE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/$<LOWER_CASE:$<CONFIG>>")
    set(MTCAD_RELEASE_ASSETS_DIR "${MTCAD_RELEASE_OUTPUT_DIR}/assets")

    add_custom_command(TARGET mtcad_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MTCAD_RELEASE_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MTCAD_RELEASE_ASSETS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:mtcad_gui>" "${MTCAD_RELEASE_OUTPUT_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:mtcad_gui> "${MTCAD_RELEASE_OUTPUT_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/gui/assets" "${MTCAD_RELEASE_ASSETS_DIR}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
endif()
