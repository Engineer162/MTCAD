set(MTCAD_IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui" CACHE PATH "Path to Dear ImGui")

if(NOT EXISTS "${MTCAD_IMGUI_DIR}/imgui.h")
    message(FATAL_ERROR "ImGui not found. Place ImGui at ${MTCAD_IMGUI_DIR} or set MTCAD_IMGUI_DIR.")
endif()

include(FetchContent)

find_package(Vulkan REQUIRED)

set(MTCAD_SDL3_TARGET "")
find_package(SDL3 QUIET CONFIG)

if(SDL3_FOUND)
    foreach(candidate_target SDL3::SDL3 SDL3::SDL3-shared SDL3::SDL3-static)
        if(TARGET ${candidate_target})
            set(MTCAD_SDL3_TARGET ${candidate_target})
            break()
        endif()
    endforeach()
endif()

if(NOT MTCAD_SDL3_TARGET)
    option(MTCAD_FETCH_SDL3 "Fetch SDL3 automatically when not found in the system" ON)
    if(MTCAD_FETCH_SDL3)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.2.10
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(SDL3)

        foreach(candidate_target SDL3::SDL3 SDL3::SDL3-shared SDL3::SDL3-static)
            if(TARGET ${candidate_target})
                set(MTCAD_SDL3_TARGET ${candidate_target})
                break()
            endif()
        endforeach()
    endif()
endif()

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)

if(NOT MTCAD_SDL3_TARGET)
    message(FATAL_ERROR
        "SDL3 target not found. Install SDL3 (SDL3Config.cmake), set SDL3_DIR/CMAKE_PREFIX_PATH, or enable MTCAD_FETCH_SDL3."
    )
endif()

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
        ${stb_SOURCE_DIR}
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
