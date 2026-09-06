#include <cstdint>
#include <cstring>
#include <array>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include "version.h"

// Kernel
#include "mtkernel/kernel.h"

// Managers
#include "managers/icon_manager.h"
#include "managers/shortcut_manager.h"
#include "managers/theme_manager.h"
#include "managers/path_manager.h"

// App
#include "runtime/icon_bootstrap.h"
#include "runtime/settings_io.h"
#include "runtime/vulkan_runtime.h"

// Windows
#include "windows/settings_window.h"
#include "windows/extrude_window.h"
#include "windows/tool_window.h"
#include "windows/toolbar_window.h"
#include "windows/viewport_window.h"
#include "windows/workspace_browser_window.h"
#include "windows/about_window.h"

// Tools
#include "tools/tools_init.h"
#include "tools/tool_manager.h"

// Imgui
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

// SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// Windows specific macros
#if defined(_WIN32)
#include <shlobj.h>
#ifndef MTCAD_PREFER_HIGH_PERFORMANCE_GPU
#define MTCAD_PREFER_HIGH_PERFORMANCE_GPU 1
#endif
#endif

// Enable high performance GPU on Windows
#if defined(_WIN32) && (MTCAD_PREFER_HIGH_PERFORMANCE_GPU != 0)
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0x00000001;
}
#endif

static float clampf(float v, float min_v, float max_v) {
    if (v < min_v) {
        return min_v;
    }
    if (v > max_v) {
        return max_v;
    }
    return v;
}

static ImGuiMouseButton pan_button_from_index(int index) {
    switch (index) {
        case 0: return ImGuiMouseButton_Left;
        case 1: return ImGuiMouseButton_Right;
        case 2: return ImGuiMouseButton_Middle;
        default: return ImGuiMouseButton_Right;
    }
}

static ImGuiMouseButton orbit_button_from_index(int index) {
    switch (index) {
        case 0: return ImGuiMouseButton_Left;
        case 1: return ImGuiMouseButton_Right;
        case 2: return ImGuiMouseButton_Middle;
        default: return ImGuiMouseButton_Right;
    }
}


enum class AppIcon {
    Settings = 0,
    File,
    Panel,
    Save,
    Undo,
    Redo,
    Help,
    Camera,
    Grid,
    Folder,
    Extrude,
    Revolve,
    Hole,
    PressPull,
    Chamfer,
    Fillet,
    Shell,
    Combine,
    SplitBody,
    Plane,
    Axes,
    Measure,
    Section,
    Interference,
    Point,
    Mirror,
    Line,
    Rectangle,
    Circle,
    Arc,
    Text,
    Count,
};

struct IconSlot {
    const char* name = "";
    std::string path;
    IconTexture texture = {};
    bool loaded = false;
};

static constexpr size_t k_app_icon_count = (size_t)AppIcon::Count;

static constexpr size_t icon_slot_index(AppIcon icon) {
    return (size_t)icon;
}

int main() {
    auto show_fatal_error = [](const char* title, const char* details) {
        const std::string message = (details != nullptr && details[0] != '\0')
            ? std::string(details)
            : std::string("No error message was provided.");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message.c_str(), nullptr);
    };

    const std::filesystem::path settings_dir = get_settings_directory();
    const std::filesystem::path user_settings_file = settings_dir / "user_settings.ini";
    const std::filesystem::path imgui_ini_file = settings_dir / "imgui.ini";

    const std::filesystem::path app_data_directory = get_app_data_directory();
    const std::string themes_directory = (app_data_directory / "themes").string();
    initialize_theme_manager(themes_directory.c_str());
    UserSettings applied_settings;
    applied_settings.window_x = SDL_WINDOWPOS_CENTERED;
    applied_settings.window_y = SDL_WINDOWPOS_CENTERED;
    mtcad::runtime::ApplyDefaultShortcuts(&applied_settings);
    mtcad::runtime::LoadUserSettingsIni(user_settings_file.string().c_str(), &applied_settings);
    mtcad::runtime::EnsureDefaultImGuiIni(settings_dir);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        show_fatal_error("MTCAD startup error", SDL_GetError());
        return 1;
    }

    const SDL_WindowFlags window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    SDL_Window* window = SDL_CreateWindow("MTCAD", applied_settings.window_width, applied_settings.window_height, window_flags);
    if (window == nullptr) {
        show_fatal_error("MTCAD startup error", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_SetWindowPosition(window, applied_settings.window_x, applied_settings.window_y);
    if (applied_settings.window_fullscreen) {
        SDL_SetWindowFullscreen(window, true);
    }
    mtcad::runtime::TrySetSdlWindowIcon(window);

    ImVector<const char*> instance_extensions;
    uint32_t sdl_extensions_count = 0;
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count);
    if (sdl_extensions == nullptr || sdl_extensions_count == 0) {
        show_fatal_error("MTCAD startup error", "Failed to query SDL Vulkan instance extensions.");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    for (uint32_t n = 0; n < sdl_extensions_count; n++) {
        instance_extensions.push_back(sdl_extensions[n]);
    }

    mtcad::runtime::SetupVulkan(instance_extensions);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!mtcad::runtime::CreateWindowSurface(window, &surface)) {
        show_fatal_error("MTCAD startup error", "Failed to create Vulkan surface.");
        SDL_DestroyWindow(window);
        mtcad::runtime::CleanupVulkan();
        SDL_Quit();
        return 1;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    ImGui_ImplVulkanH_Window* wd = mtcad::runtime::GetMainWindowData();
    mtcad::runtime::SetupVulkanWindow(wd, surface, width, height);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    static std::string imgui_ini_path = imgui_ini_file.string();
    io.IniFilename = imgui_ini_path.c_str();

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    apply_imgui_theme(applied_settings.theme_index);

    ImGui_ImplSDL3_InitForVulkan(window);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = mtcad::runtime::GetInstance();
    init_info.PhysicalDevice = mtcad::runtime::GetPhysicalDevice();
    init_info.Device = mtcad::runtime::GetDevice();
    init_info.QueueFamily = mtcad::runtime::GetQueueFamily();
    init_info.Queue = mtcad::runtime::GetQueue();
    init_info.PipelineCache = mtcad::runtime::GetPipelineCache();
    init_info.DescriptorPool = mtcad::runtime::GetDescriptorPool();
    init_info.MinImageCount = mtcad::runtime::GetMinImageCount();
    init_info.ImageCount = wd->ImageCount;
    init_info.Allocator = mtcad::runtime::GetAllocator();
    init_info.ApiVersion = VK_API_VERSION_1_1;
    init_info.MinAllocationSize = 1024 * 1024;
    init_info.PipelineInfoMain.RenderPass = wd->RenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = mtcad::runtime::CheckVkResult;
    ImGui_ImplVulkan_Init(&init_info);

    ImVec4 selected_theme_icon_tint;
    if (get_theme_icon_tint(applied_settings.theme_index, &selected_theme_icon_tint)) {
        set_icon_loader_black_recolor(&selected_theme_icon_tint);
    } else {
        set_icon_loader_black_recolor(nullptr);
    }

    std::array<IconSlot, k_app_icon_count> icons = {{
        {"settings"},
        {"file"},
        {"panel"},
        {"save"},
        {"undo"},
        {"redo"},
        {"help"},
        {"camera"},
        {"grid"},
        {"folder"},
        {"extrude"},
        {"revolve"},
        {"hole"},
        {"press_pull"},
        {"chamfer"},
        {"fillet"},
        {"shell"},
        {"combine"},
        {"split_body"},
        {"plane"},
        {"axes"},
        {"measure"},
        {"section"},
        {"interference"},
        {"point"},
        {"mirror"},
        {"line"},
        {"rectangle"},
        {"circle"},
        {"arc"},
        {"text"},
    }};

    auto icon_slot = [&](AppIcon icon) -> IconSlot& {
        return icons[icon_slot_index(icon)];
    };

    for (IconSlot& slot : icons) {
        slot.path = mtcad::runtime::ResolveIconPath(slot.name);
        if (!slot.path.empty()) {
            slot.loaded = load_icon_texture_from_file(
                slot.path.c_str(),
                mtcad::runtime::GetPhysicalDevice(),
                mtcad::runtime::GetDevice(),
                mtcad::runtime::GetQueueFamily(),
                mtcad::runtime::GetQueue(),
                mtcad::runtime::GetAllocator(),
                &slot.texture);
        }
    }

    ImVec4 clear_color(0.10f, 0.11f, 0.13f, 1.0f);

    mtcad::InitializeTools();

    WorkspaceBrowserWindow workspace_browser_window;
    ToolbarWindow toolbar_window;
    ViewportWindow viewport_window;
    SettingsWindow settings_window;
    AboutWindow about_window;
    mtcad::SketchPaletteWindow tool_window;
    mtcad::ExtrudePaletteWindow extrude_window;

    auto apply_loaded_icon_textures = [&]() {
        struct ToolbarIconBinding {
            AppIcon icon;
            ToolbarWindow::IconId toolbar_icon;
        };

        const std::array<ToolbarIconBinding, 21> toolbar_icon_bindings = {{
            {AppIcon::Extrude, ToolbarWindow::IconId::Extrude},
            {AppIcon::Revolve, ToolbarWindow::IconId::Revolve},
            {AppIcon::Hole, ToolbarWindow::IconId::Hole},
            {AppIcon::PressPull, ToolbarWindow::IconId::PressPull},
            {AppIcon::Chamfer, ToolbarWindow::IconId::Chamfer},
            {AppIcon::Fillet, ToolbarWindow::IconId::Fillet},
            {AppIcon::Shell, ToolbarWindow::IconId::Shell},
            {AppIcon::Combine, ToolbarWindow::IconId::Combine},
            {AppIcon::SplitBody, ToolbarWindow::IconId::SplitBody},
            {AppIcon::Plane, ToolbarWindow::IconId::Plane},
            {AppIcon::Axes, ToolbarWindow::IconId::Axes},
            {AppIcon::Point, ToolbarWindow::IconId::Point},
            {AppIcon::Measure, ToolbarWindow::IconId::Measure},
            {AppIcon::Interference, ToolbarWindow::IconId::Interference},
            {AppIcon::Section, ToolbarWindow::IconId::Section},
            {AppIcon::Line, ToolbarWindow::IconId::Line},
            {AppIcon::Rectangle, ToolbarWindow::IconId::Rectangle},
            {AppIcon::Circle, ToolbarWindow::IconId::Circle},
            {AppIcon::Arc, ToolbarWindow::IconId::Arc},
            {AppIcon::Text, ToolbarWindow::IconId::Text},
            {AppIcon::Mirror, ToolbarWindow::IconId::Mirror},
        }};

        auto icon_texture_id = [&](AppIcon icon) -> ImTextureID {
            const IconSlot& slot = icon_slot(icon);
            return slot.loaded ? (ImTextureID)slot.texture.descriptor_set : (ImTextureID)0;
        };

        viewport_window.SetSettingsIconTexture(icon_texture_id(AppIcon::Settings));
        viewport_window.SetCameraIconTexture(icon_texture_id(AppIcon::Camera));
        viewport_window.SetGridIconTexture(icon_texture_id(AppIcon::Grid));
        workspace_browser_window.SetFolderIconTexture(icon_texture_id(AppIcon::Folder));

        for (const ToolbarIconBinding& binding : toolbar_icon_bindings) {
            toolbar_window.SetIconTexture(binding.toolbar_icon, icon_texture_id(binding.icon));
        }
    };

    float ui_text_scale = applied_settings.text_scale;
    float ui_icon_scale = applied_settings.icon_scale;
    viewport_window.SetPanButton(pan_button_from_index(applied_settings.viewport_pan_button));
    viewport_window.SetOrbitButton(orbit_button_from_index(applied_settings.viewport_orbit_button));
    apply_loaded_icon_textures();
    toolbar_window.SetIconScale(ui_icon_scale);
    workspace_browser_window.SetRootDirectory(applied_settings.workspace_root);
    if (applied_settings.keyboard_navigation_enabled) {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    }

    UserSettings pending_settings = applied_settings;
    int current_icon_theme_index = clamp_theme_index(applied_settings.theme_index);
    bool was_settings_window_open = false;

    using WindowRenderer = std::function<void(const ImGuiIO&)>;
    std::vector<WindowRenderer> window_renderers;
    window_renderers.emplace_back([&workspace_browser_window](const ImGuiIO& frame_io) {
        workspace_browser_window.Render(frame_io);
    });
    window_renderers.emplace_back([&toolbar_window](const ImGuiIO& frame_io) {
        toolbar_window.Render(frame_io);
    });
    window_renderers.emplace_back([&viewport_window](const ImGuiIO& frame_io) {
        viewport_window.Render(frame_io);
    });
    window_renderers.emplace_back([&about_window, window](const ImGuiIO&) {
        about_window.Render(window);
    });

    bool show_settings_window = false;
    bool shortcut_new_was_down = false;
    bool shortcut_open_was_down = false;
    bool shortcut_save_was_down = false;
    bool shortcut_undo_was_down = false;
    bool shortcut_redo_was_down = false;
    bool shortcut_settings_was_down = false;
    bool shortcut_about_was_down = false;
    bool shortcut_create_sketch_was_down = false;
    bool shortcut_finish_sketch_was_down = false;
    bool shortcut_extrude_was_down = false;
    bool shortcut_revolve_was_down = false;
    bool shortcut_line_was_down = false;
    bool shortcut_rectangle_was_down = false;
    bool shortcut_circle_was_down = false;

    bool done = false;
    bool showed_vulkan_fatal_message = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                done = true;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
                done = true;
            }
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        int fb_width = 0;
        int fb_height = 0;
        SDL_GetWindowSizeInPixels(window, &fb_width, &fb_height);
        mtcad::runtime::RebuildSwapchainIfNeeded(wd, fb_width, fb_height);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        io.FontGlobalScale = ui_text_scale;

        const bool shortcut_capture_active = settings_window.IsCapturingShortcut();
        const bool shortcuts_blocked = shortcut_capture_active || io.WantTextInput;
        auto consume_shortcut_press = [&](const ShortcutChord& chord, bool* was_down) {
            const bool is_down = !shortcuts_blocked && IsShortcutDown(chord);
            const bool fired = is_down && !(*was_down);
            *was_down = is_down;
            return fired;
        };

        const bool shortcut_new_pressed = consume_shortcut_press(applied_settings.shortcut_new, &shortcut_new_was_down);
        const bool shortcut_open_pressed = consume_shortcut_press(applied_settings.shortcut_open, &shortcut_open_was_down);
        const bool shortcut_save_pressed = consume_shortcut_press(applied_settings.shortcut_save, &shortcut_save_was_down);
        const bool shortcut_undo_pressed = consume_shortcut_press(applied_settings.shortcut_undo, &shortcut_undo_was_down);
        const bool shortcut_redo_pressed = consume_shortcut_press(applied_settings.shortcut_redo, &shortcut_redo_was_down);
        const bool shortcut_settings_pressed = consume_shortcut_press(applied_settings.shortcut_settings, &shortcut_settings_was_down);
        const bool shortcut_about_pressed = consume_shortcut_press(applied_settings.shortcut_about, &shortcut_about_was_down);
        const bool shortcut_create_sketch_pressed = consume_shortcut_press(applied_settings.shortcut_create_sketch, &shortcut_create_sketch_was_down);
        const bool shortcut_finish_sketch_pressed = consume_shortcut_press(applied_settings.shortcut_finish_sketch, &shortcut_finish_sketch_was_down);
        const bool shortcut_extrude_pressed = consume_shortcut_press(applied_settings.shortcut_extrude, &shortcut_extrude_was_down);
        const bool shortcut_revolve_pressed = consume_shortcut_press(applied_settings.shortcut_revolve, &shortcut_revolve_was_down);
        const bool shortcut_line_pressed = consume_shortcut_press(applied_settings.shortcut_line, &shortcut_line_was_down);
        const bool shortcut_rectangle_pressed = consume_shortcut_press(applied_settings.shortcut_rectangle, &shortcut_rectangle_was_down);
        const bool shortcut_circle_pressed = consume_shortcut_press(applied_settings.shortcut_circle, &shortcut_circle_was_down);

        if (shortcut_settings_pressed) {
            show_settings_window = true;
        }
        if (shortcut_about_pressed) {
            about_window.SetOpen(true);
        }
        if (shortcut_create_sketch_pressed) {
            toolbar_window.RequestBeginSketchMode();
            tool_window.Open();
        }
        if (shortcut_finish_sketch_pressed) {
            toolbar_window.RequestBeginSolidMode();
        }
        if (shortcut_extrude_pressed) {
            toolbar_window.RequestSelectTool("Extrude");
        }
        if (shortcut_revolve_pressed) {
            toolbar_window.RequestSelectTool("Revolve");
        }
        if (toolbar_window.IsSketchMode() && shortcut_line_pressed) {
            toolbar_window.RequestSelectTool("Line");
        }
        if (toolbar_window.IsSketchMode() && shortcut_rectangle_pressed) {
            toolbar_window.RequestSelectTool("Rectangle");
        }
        if (toolbar_window.IsSketchMode() && shortcut_circle_pressed) {
            toolbar_window.RequestSelectTool("Circle");
        }

        const std::string shortcut_new_label = ShortcutToDisplayString(applied_settings.shortcut_new);
        const std::string shortcut_open_label = ShortcutToDisplayString(applied_settings.shortcut_open);
        const std::string shortcut_save_label = ShortcutToDisplayString(applied_settings.shortcut_save);
        const std::string shortcut_undo_label = ShortcutToDisplayString(applied_settings.shortcut_undo);
        const std::string shortcut_redo_label = ShortcutToDisplayString(applied_settings.shortcut_redo);
        const std::string shortcut_about_label = ShortcutToDisplayString(applied_settings.shortcut_about);

        bool trigger_new_action = shortcut_new_pressed;
        bool trigger_open_action = shortcut_open_pressed;
        bool trigger_save_action = shortcut_save_pressed;
        bool trigger_undo_action = shortcut_undo_pressed;
        bool trigger_redo_action = shortcut_redo_pressed;

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        const ImVec2 navbar_button_padding(4.0f, 3.0f);
        const float requested_icon_size = 16.0f * ui_icon_scale;

        const float default_menu_button_height = ImGui::GetFrameHeight();
        float scaled_menu_button_height = requested_icon_size + navbar_button_padding.y * 2.0f;
        if (scaled_menu_button_height < default_menu_button_height) {
            scaled_menu_button_height = default_menu_button_height;
        }
        if (scaled_menu_button_height < 18.0f) {
            scaled_menu_button_height = 18.0f;
        }

        ImGui::SetNextWindowPos(main_viewport->WorkPos);
        ImGui::SetNextWindowSize(main_viewport->WorkSize);
        ImGui::SetNextWindowViewport(main_viewport->ID);

        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.WindowPadding.x, style.WindowPadding.y));
        ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
        host_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        ImGui::Begin("MTCAD Workspace", nullptr, host_flags);
        ImGuiID dockspace_id = ImGui::GetID("MTCAD_Dockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();
        ImGui::PopStyleVar();

        float menu_bar_frame_padding_y = (scaled_menu_button_height - ImGui::GetFontSize()) * 0.5f;
        if (menu_bar_frame_padding_y < style.FramePadding.y) {
            menu_bar_frame_padding_y = style.FramePadding.y;
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, menu_bar_frame_padding_y));

        if (ImGui::BeginMainMenuBar())
        {
            // Global Navbar button variables
            const ImVec2 button_padding = navbar_button_padding;
            const ImVec2 dropdown_button_padding(button_padding.x + 3.0f, button_padding.y);
            float button_height = ImGui::GetFrameHeight();
            const float min_scaled_button_height = requested_icon_size + button_padding.y * 2.0f;
            if (button_height < min_scaled_button_height) {
                button_height = min_scaled_button_height;
            }
            if (button_height < 18.0f) {
                button_height = 18.0f;
            }

            const float max_icon_size = button_height - button_padding.y * 2.0f;
            float save_icon_size = requested_icon_size;
            if (save_icon_size > max_icon_size) {
                save_icon_size = max_icon_size;
            }
            if (save_icon_size < 12.0f) {
                save_icon_size = 12.0f;
            }

            const float dropdown_arrow_scale = 0.15f;
            const float dropdown_arrow_min_size = 3.0f;

            auto compute_dropdown_arrow_slot_width = [&](float icon_size) {
                const float min_arrow_slot = 5.0f;
                const float icon_based_arrow_slot = icon_size * 0.375f;
                return (icon_based_arrow_slot > min_arrow_slot) ? icon_based_arrow_slot : min_arrow_slot;
            };

            auto compute_dropdown_icon_button_width = [&](float icon_size) {
                return icon_size + dropdown_button_padding.x * 2.0f + compute_dropdown_arrow_slot_width(icon_size);
            };

            auto draw_dropdown_arrow = [&]() {
                const ImVec2 item_min = ImGui::GetItemRectMin();
                const ImVec2 item_max = ImGui::GetItemRectMax();
                const float item_height = item_max.y - item_min.y;
                float arrow_size = item_height * dropdown_arrow_scale;
                if (arrow_size < dropdown_arrow_min_size) {
                    arrow_size = dropdown_arrow_min_size;
                }
                const float right_gutter = ImGui::GetStyle().FramePadding.x * 0.55f;
                const float arrow_cx = item_max.x - right_gutter - arrow_size;
                const float arrow_cy = item_min.y + item_height * 0.5f + 0.5f;
                ImGui::GetWindowDrawList()->AddTriangleFilled(
                    ImVec2(arrow_cx - arrow_size, arrow_cy - arrow_size * 0.6f),
                    ImVec2(arrow_cx + arrow_size, arrow_cy - arrow_size * 0.6f),
                    ImVec2(arrow_cx, arrow_cy + arrow_size * 0.7f),
                    ImGui::GetColorU32(ImGuiCol_Text));
            };

            auto render_dropdown_icon_button = [&](const char* id, ImTextureID texture, float icon_size, const char* tooltip) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, dropdown_button_padding);
                const float arrow_slot_width = compute_dropdown_arrow_slot_width(icon_size);
                const float dropdown_button_width = compute_dropdown_icon_button_width(icon_size);
                const bool clicked = ImGui::Button(id, ImVec2(dropdown_button_width, button_height));

                const ImVec2 item_min = ImGui::GetItemRectMin();
                const ImVec2 item_max = ImGui::GetItemRectMax();
                const float item_height = item_max.y - item_min.y;
                float arrow_size = item_height * dropdown_arrow_scale;
                if (arrow_size < dropdown_arrow_min_size) {
                    arrow_size = dropdown_arrow_min_size;
                }

                const float right_gutter = ImGui::GetStyle().FramePadding.x * 0.55f;
                const float arrow_center_x = item_max.x - right_gutter - arrow_size;
                const float arrow_left_x = arrow_center_x - arrow_size;
                const float icon_area_min_x = item_min.x + dropdown_button_padding.x;
                const float icon_area_max_x = item_max.x - dropdown_button_padding.x - arrow_slot_width;
                float draw_icon_size = icon_size;
                const float icon_area_width = icon_area_max_x - icon_area_min_x;
                if (icon_area_width < draw_icon_size) {
                    draw_icon_size = icon_area_width;
                }
                const float max_icon_height = item_height - dropdown_button_padding.y * 2.0f;
                if (draw_icon_size > max_icon_height) {
                    draw_icon_size = max_icon_height;
                }
                if (draw_icon_size < 10.0f) {
                    draw_icon_size = 10.0f;
                }
                float icon_x = icon_area_min_x;
                if (icon_area_max_x > icon_area_min_x) {
                    if (icon_area_width > draw_icon_size) {
                        icon_x = icon_area_min_x + (icon_area_width - draw_icon_size) * 0.5f;
                    }
                }
                const float icon_y = item_min.y + (item_height - draw_icon_size) * 0.5f;

                ImGui::GetWindowDrawList()->AddImage(
                    texture,
                    ImVec2(icon_x, icon_y),
                    ImVec2(icon_x + draw_icon_size, icon_y + draw_icon_size),
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f),
                    ImGui::GetColorU32(get_icon_tint()));

                draw_dropdown_arrow();
                ImGui::PopStyleVar();

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", tooltip);
                }
                return clicked;
            };

            auto render_icon_button = [&](const char* id, ImTextureID texture, float icon_size, const ImVec2& frame_padding, const char* tooltip) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, frame_padding);
                const float button_width = icon_size + frame_padding.x * 2.0f;
                const bool clicked = ImGui::Button(id, ImVec2(button_width, button_height));

                const ImVec2 item_min = ImGui::GetItemRectMin();
                const ImVec2 item_max = ImGui::GetItemRectMax();
                const float item_height = item_max.y - item_min.y;
                float draw_icon_size = icon_size;
                const float max_icon_height = item_height - frame_padding.y * 2.0f;
                if (draw_icon_size > max_icon_height) {
                    draw_icon_size = max_icon_height;
                }
                if (draw_icon_size < 10.0f) {
                    draw_icon_size = 10.0f;
                }

                const float icon_x = item_min.x + (button_width - draw_icon_size) * 0.5f;
                const float icon_y = item_min.y + (item_height - draw_icon_size) * 0.5f;
                ImGui::GetWindowDrawList()->AddImage(
                    texture,
                    ImVec2(icon_x, icon_y),
                    ImVec2(icon_x + draw_icon_size, icon_y + draw_icon_size),
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f),
                    ImGui::GetColorU32(get_icon_tint()));

                ImGui::PopStyleVar();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", tooltip);
                }
                return clicked;
            };

            // File button
            float file_icon_size = requested_icon_size;
            if (file_icon_size > max_icon_size) {
                file_icon_size = max_icon_size;
            }
            if (file_icon_size < 12.0f) {
                file_icon_size = 12.0f;
            }

            const char* file_fallback_label = "File";
            const IconSlot& file_icon = icon_slot(AppIcon::File);
            const bool has_file_icon = file_icon.loaded && file_icon.texture.descriptor_set != VK_NULL_HANDLE;
            bool open_file_popup = trigger_new_action || trigger_open_action || trigger_save_action;

            if (has_file_icon) {
                if (render_dropdown_icon_button("##file_icon", (ImTextureID)file_icon.texture.descriptor_set, file_icon_size, "File")) {
                    open_file_popup = true;
                }
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, dropdown_button_padding);
                if (ImGui::Button(file_fallback_label, ImVec2(0.0f, button_height))) {
                    open_file_popup = true;
                }
                draw_dropdown_arrow();
                ImGui::PopStyleVar();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("File");
                }
            }

            if (open_file_popup) {
                ImGui::OpenPopup("FilePopup");
            }
            if (ImGui::BeginPopup("FilePopup")) {
                if (ImGui::MenuItem("New", shortcut_new_label.c_str()) || trigger_new_action) {
                    //create_new_show();
                    trigger_new_action = false;
                }
                if (ImGui::MenuItem("Open...", shortcut_open_label.c_str()) || trigger_open_action) {
                    //request_open_show_dialog();
                    trigger_open_action = false;
                }
                if (ImGui::MenuItem("Save", shortcut_save_label.c_str()) || trigger_save_action) {
                    //save_current_show(false);
                    trigger_save_action = false;
                }
                if (ImGui::MenuItem("Save As...")) {
                    //request_save_show_as_dialog(false);
                }
                ImGui::EndPopup();
            }

            // Panel button
            float panel_icon_size = requested_icon_size;
            if (panel_icon_size > max_icon_size) {
                panel_icon_size = max_icon_size;
            }
            if (panel_icon_size < 12.0f) {
                panel_icon_size = 12.0f;
            }

            const char* panel_fallback_label = "Panel";
            const IconSlot& panel_icon = icon_slot(AppIcon::Panel);
            const bool has_panel_icon = panel_icon.loaded && panel_icon.texture.descriptor_set != VK_NULL_HANDLE;
            bool open_panel_popup = false;

            if (has_panel_icon) {
                if (render_dropdown_icon_button("##panel_icon", (ImTextureID)panel_icon.texture.descriptor_set, panel_icon_size, "Panel")) {
                    open_panel_popup = true;
                }
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, dropdown_button_padding);
                if (ImGui::Button(panel_fallback_label, ImVec2(0.0f, button_height))) {
                    open_panel_popup = true;
                }
                draw_dropdown_arrow();
                ImGui::PopStyleVar();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Panel");
                }
            }

            if (open_panel_popup) {
                ImGui::OpenPopup("PanelPopup");
            }
            if (ImGui::BeginPopup("PanelPopup")) {
                ImGui::Separator();
                ImGui::TextUnformatted("General:");
                ImGui::Separator();
                if (ImGui::MenuItem("Workspace Browser", nullptr, workspace_browser_window.IsOpen())) {
                    workspace_browser_window.SetOpen(!workspace_browser_window.IsOpen());
                }
                ImGui::Separator();
                ImGui::TextUnformatted("Modeling windows:");
                ImGui::Separator();
                if (ImGui::BeginMenu("Modeling Panels")) {
                    if (ImGui::MenuItem("Viewport", nullptr, viewport_window.IsOpen())) {
                        viewport_window.SetOpen(!viewport_window.IsOpen());
                    }
                    if (ImGui::MenuItem("Toolbar", nullptr, toolbar_window.IsOpen())) {
                        toolbar_window.SetOpen(!toolbar_window.IsOpen());
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                ImGui::TextUnformatted("Helper windows/tools:");
                ImGui::Separator();
                if (ImGui::MenuItem("Close All Panels")) {
                    workspace_browser_window.SetOpen(false);
                    viewport_window.SetOpen(false);
                    toolbar_window.SetOpen(false);
                }
                ImGui::EndPopup();
            }

            // Save button
            const char* save_fallback_label = "Save";
            const IconSlot& save_icon = icon_slot(AppIcon::Save);
            const bool has_save_icon = save_icon.loaded && save_icon.texture.descriptor_set != VK_NULL_HANDLE;

            if (has_save_icon) {
                if (render_icon_button("##save_icon", (ImTextureID)save_icon.texture.descriptor_set, save_icon_size, button_padding, "Save")) {
                    // Do save stuff here
                }
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, button_padding);
                if (ImGui::Button(save_fallback_label, ImVec2(0.0f, button_height))) {
                    // Do save stuff here
                }
                ImGui::PopStyleVar();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Save");
                }
            }

            // Undo button
            float undo_icon_size = requested_icon_size;
            if (undo_icon_size > max_icon_size) {
                undo_icon_size = max_icon_size;
            }
            if (undo_icon_size < 12.0f) {
                undo_icon_size = 12.0f;
            }

            const char* undo_fallback_label = "Undo";
            const IconSlot& Undo_icon = icon_slot(AppIcon::Undo);
            const bool has_Undo_icon = Undo_icon.loaded && Undo_icon.texture.descriptor_set != VK_NULL_HANDLE;
            bool open_undo_popup = false;

            if (has_Undo_icon) {
                if (render_dropdown_icon_button("##undo_icon", (ImTextureID)Undo_icon.texture.descriptor_set, undo_icon_size, "Undo")) {
                    open_undo_popup = true;
                }
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, dropdown_button_padding);
                if (ImGui::Button(undo_fallback_label, ImVec2(0.0f, button_height))) {
                    open_undo_popup = true;
                }
                draw_dropdown_arrow();
                ImGui::PopStyleVar();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Undo");
                }
            }

            if (open_undo_popup) {
                ImGui::OpenPopup("UndoPopup");
            }
            if (trigger_undo_action) {
                ImGui::OpenPopup("UndoPopup");
                trigger_undo_action = false;
            }
            if (ImGui::BeginPopup("UndoPopup")) {
                if (ImGui::MenuItem("Undo", shortcut_undo_label.c_str())) {
                    // TODO: route to undo command stack.
                }
                ImGui::Separator();
                ImGui::MenuItem("No actions in history", nullptr, false, false);
                ImGui::EndPopup();
            }

            // Redo button
            float redo_icon_size = requested_icon_size;
            if (redo_icon_size > max_icon_size) {
                redo_icon_size = max_icon_size;
            }
            if (redo_icon_size < 12.0f) {
                redo_icon_size = 12.0f;
            }

            const char* redo_fallback_label = "Redo";
            const IconSlot& redo_icon = icon_slot(AppIcon::Redo);
            const bool has_redo_icon = redo_icon.loaded && redo_icon.texture.descriptor_set != VK_NULL_HANDLE;
            bool open_redo_popup = false;

            if (has_redo_icon) {
                if (render_dropdown_icon_button("##redo_icon", (ImTextureID)redo_icon.texture.descriptor_set, redo_icon_size, "Redo")) {
                    open_redo_popup = true;
                }
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, dropdown_button_padding);
                if (ImGui::Button(redo_fallback_label, ImVec2(0.0f, button_height))) {
                    open_redo_popup = true;
                }
                draw_dropdown_arrow();
                ImGui::PopStyleVar();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Redo");
                }
            }

            if (open_redo_popup) {
                ImGui::OpenPopup("RedoPopup");
            }
            if (trigger_redo_action) {
                ImGui::OpenPopup("RedoPopup");
                trigger_redo_action = false;
            }
            if (ImGui::BeginPopup("RedoPopup")) {
                if (ImGui::MenuItem("Redo", shortcut_redo_label.c_str())) {
                    // TODO: route to redo command stack.
                }
                ImGui::Separator();
                ImGui::MenuItem("No actions in history", nullptr, false, false);
                ImGui::EndPopup();
            }

            // Right window padding

            // Settings button padding and sizing
            float settings_icon_size = requested_icon_size;
            if (settings_icon_size > max_icon_size) {
                settings_icon_size = max_icon_size;
            }
            if (settings_icon_size < 12.0f) {
                settings_icon_size = 12.0f;
            }

            const char* settings_fallback_label = "Cfg";
            const float fallback_width = ImGui::CalcTextSize(settings_fallback_label).x + button_padding.x * 2.0f;
            const float settings_icon_button_width = settings_icon_size + button_padding.x * 2.0f;
            const IconSlot& settings_icon = icon_slot(AppIcon::Settings);
            const bool has_settings_icon = settings_icon.loaded && settings_icon.texture.descriptor_set != VK_NULL_HANDLE;
            const float settings_button_width = has_settings_icon ? settings_icon_button_width : fallback_width;
            const float settings_right_padding = ImGui::GetStyle().FramePadding.x;
            const float settings_x = ImGui::GetWindowContentRegionMax().x - settings_button_width - settings_right_padding;

            // Help button padding and sizing
            const ImVec2 help_button_padding = dropdown_button_padding;
            float help_button_height = button_height;
            const float help_max_icon_size = help_button_height - help_button_padding.y * 2.0f;
            float help_icon_size = requested_icon_size;
            if (help_icon_size > help_max_icon_size) {
                help_icon_size = help_max_icon_size;
            }
            if (help_icon_size < 12.0f) {
                help_icon_size = 12.0f;
            }

            const char* help_fallback_label = "Help";
            const IconSlot& help_icon = icon_slot(AppIcon::Help);
            const bool has_help_icon = help_icon.loaded && help_icon.texture.descriptor_set != VK_NULL_HANDLE;
            const float help_button_width = has_help_icon
                ? compute_dropdown_icon_button_width(help_icon_size)
                : ImGui::CalcTextSize(help_fallback_label).x + help_button_padding.x * 2.0f;
            const float help_right_padding = ImGui::GetStyle().FramePadding.x;
            const float help_x = settings_x - help_button_width - help_right_padding;
            bool open_help_popup = false;
            ImGui::SetCursorPosX(help_x);

            // Help button
            if (has_help_icon) {
                if (render_dropdown_icon_button("##help_icon", (ImTextureID)help_icon.texture.descriptor_set, help_icon_size, "Help")) {
                    open_help_popup = true;
                }
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, help_button_padding);
                if (ImGui::Button(help_fallback_label, ImVec2(0.0f, help_button_height))) {
                    open_help_popup = true;
                }
                draw_dropdown_arrow();
                ImGui::PopStyleVar();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Help");
                }
            }

            if (open_help_popup) {
                ImGui::OpenPopup("HelpButtonPopup");
            }
            if (ImGui::BeginPopup("HelpButtonPopup")) {
                if (ImGui::MenuItem("About", shortcut_about_label.c_str()) || shortcut_about_pressed) {
                    about_window.SetOpen(true);
                }
                ImGui::EndPopup();
            }

            // Settings button
            ImGui::SetCursorPosX(settings_x);
            if (has_settings_icon) {
                if (render_icon_button("##settings_icon", (ImTextureID)settings_icon.texture.descriptor_set, settings_icon_size, button_padding, "Settings")) {
                    show_settings_window = true;
                }
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, button_padding);
                if (ImGui::Button(settings_fallback_label, ImVec2(0.0f, button_height))) {
                    show_settings_window = true;
                }
                ImGui::PopStyleVar();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Settings");
                }
            }
            ImGui::EndMainMenuBar();
        }
        ImGui::PopStyleVar();

        if (show_settings_window && !was_settings_window_open) {
            pending_settings = applied_settings;
        }

        if (show_settings_window) {
            pending_settings.text_scale = clampf(pending_settings.text_scale, 0.80f, 2.00f);
            pending_settings.icon_scale = clampf(pending_settings.icon_scale, 0.80f, 2.00f);
            pending_settings.theme_index = clamp_theme_index(pending_settings.theme_index);
            ui_text_scale = pending_settings.text_scale;
            ui_icon_scale = pending_settings.icon_scale;
            apply_imgui_theme(pending_settings.theme_index);

            bool settings_open = show_settings_window;
            const SettingsWindowResult settings_result = settings_window.Render(&settings_open, &pending_settings);

            if (settings_result.apply_pressed) {
                pending_settings.text_scale = clampf(pending_settings.text_scale, 0.80f, 2.00f);
                pending_settings.icon_scale = clampf(pending_settings.icon_scale, 0.80f, 2.00f);
                pending_settings.theme_index = clamp_theme_index(pending_settings.theme_index);
                if (pending_settings.viewport_pan_button < 0 || pending_settings.viewport_pan_button > 2) {
                    pending_settings.viewport_pan_button = 0;
                }
                if (pending_settings.viewport_orbit_button < 0 || pending_settings.viewport_orbit_button > 2) {
                    pending_settings.viewport_orbit_button = 1;
                }

                applied_settings = pending_settings;
                ui_text_scale = applied_settings.text_scale;
                ui_icon_scale = applied_settings.icon_scale;
                apply_imgui_theme(applied_settings.theme_index);
                viewport_window.SetPanButton(pan_button_from_index(applied_settings.viewport_pan_button));
                viewport_window.SetOrbitButton(orbit_button_from_index(applied_settings.viewport_orbit_button));
                workspace_browser_window.SetRootDirectory(applied_settings.workspace_root);

                if (applied_settings.keyboard_navigation_enabled) {
                    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                } else {
                    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
                }

                mtcad::runtime::SaveUserSettingsIni(user_settings_file.string().c_str(), applied_settings);
            }

            if (settings_result.cancel_pressed || !settings_open) {
                pending_settings = applied_settings;
                ui_text_scale = applied_settings.text_scale;
                ui_icon_scale = applied_settings.icon_scale;
                apply_imgui_theme(applied_settings.theme_index);
            }
            show_settings_window = settings_open;
        }

        was_settings_window_open = show_settings_window;

        const int desired_icon_theme_index = clamp_theme_index(show_settings_window ? pending_settings.theme_index : applied_settings.theme_index);
        if (desired_icon_theme_index != current_icon_theme_index) {
            ImVec4 icon_tint;
            if (get_theme_icon_tint(desired_icon_theme_index, &icon_tint)) {
                set_icon_loader_black_recolor(&icon_tint);
            } else {
                set_icon_loader_black_recolor(nullptr);
            }
            current_icon_theme_index = desired_icon_theme_index;
        }

        toolbar_window.SetIconScale(ui_icon_scale);

        for (const WindowRenderer& render_window : window_renderers) {
            render_window(io);
        }

        if (toolbar_window.ConsumeBeginSketchRequest()) {
            viewport_window.SetAwaitingSketchPlaneSelection(true);
            extrude_window.Close();
        }

        if (toolbar_window.ConsumeBeginSolidModeRequest()) {
            toolbar_window.SetSketchMode(false);
            viewport_window.FinishSketchMode();
            viewport_window.SetSolidMode(true);
            tool_window.Close();
            extrude_window.Close();
        }
        
        ViewportWindow::SketchPlane selected_plane = ViewportWindow::SketchPlane_None;
        if (viewport_window.ConsumeSelectedSketchPlane(&selected_plane)) {
            toolbar_window.SetSketchMode(true);
            viewport_window.SetSketchMode(true, selected_plane);
            tool_window.Open();
            extrude_window.Close();
        }

        const char* selected_tool_name = nullptr;
        if (toolbar_window.ConsumeSelectedTool(&selected_tool_name)) {
            if (selected_tool_name != nullptr && std::strcmp(selected_tool_name, "Line") == 0) {
                extrude_window.Close();
                viewport_window.StartLineDrawing();
            } else if (selected_tool_name != nullptr && std::strcmp(selected_tool_name, "Rectangle") == 0) {
                extrude_window.Close();
                viewport_window.StartRectangleDrawing();
            } else if (selected_tool_name != nullptr && std::strcmp(selected_tool_name, "Circle") == 0) {
                extrude_window.Close();
                viewport_window.StartCircleDrawing();
            } else if (selected_tool_name != nullptr && std::strcmp(selected_tool_name, "Extrude") == 0) {
                tool_window.Close();
                extrude_window.ClearSourceProfiles();
                extrude_window.Open();
            } else {
                extrude_window.Close();
                viewport_window.CancelLineDrawing();
            }
        }

        if (tool_window.ConsumeFinishSketch()) {
            toolbar_window.SetSketchMode(false);
            viewport_window.FinishSketchMode();
            viewport_window.SetSolidMode(true);
            tool_window.Close();
            extrude_window.Close();
        }

        if (tool_window.ConsumeCancelSketch()) {
            toolbar_window.SetSketchMode(false);
            viewport_window.CancelSketchMode();
            viewport_window.SetSolidMode(true);
            tool_window.Close();
            extrude_window.Close();
        }

        if (extrude_window.ConsumeApplyExtrude()) {
            bool applied_body = false;
            if (extrude_window.HasAppliedPreviewBodies()) {
                std::vector<std::vector<ViewportWindow::Vec3>> body_polygons;
                const auto& preview_polygons = extrude_window.GetAppliedPreviewBodyPolygonsWorld();
                body_polygons.reserve(preview_polygons.size());
                for (const auto& preview_points : preview_polygons) {
                    std::vector<ViewportWindow::Vec3> body_points;
                    body_points.reserve(preview_points.size());
                    for (const auto& point : preview_points) {
                        body_points.push_back({point.x, point.y, point.z});
                    }
                    if (body_points.size() >= 3) {
                        body_polygons.push_back(body_points);
                    }
                }
                if (!body_polygons.empty()) {
                    viewport_window.SetExtrudedBodyFinalBatch(body_polygons, extrude_window.GetAppliedPreviewBodyDepthWorld());
                    applied_body = true;
                }
            }
            viewport_window.ClearExtrudedBodyPreview();
            if (!applied_body) {
                viewport_window.ClearExtrudedBodyFinal();
            }
            extrude_window.Close();
        }

        if (extrude_window.ConsumeCancelExtrude()) {
            extrude_window.Close();
        }

        ImVec2 viewport_canvas_pos;
        ImVec2 viewport_canvas_size;
        if (viewport_window.GetCanvasRect(&viewport_canvas_pos, &viewport_canvas_size)) {
            std::vector<int> selected_fill_ids;
            std::vector<std::vector<ViewportWindow::Vec3>> selected_polygons_world_points;
            if (extrude_window.IsOpen()) {
                extrude_window.ClearSourceProfiles();
                if (viewport_window.GetSelectedFillProfiles(&selected_fill_ids, &selected_polygons_world_points)) {
                    for (size_t i = 0; i < selected_polygons_world_points.size(); ++i) {
                        std::vector<mtcad::ExtrudePoint3D> source_points;
                        source_points.reserve(selected_polygons_world_points[i].size());
                        for (const auto& point : selected_polygons_world_points[i]) {
                            source_points.push_back({point.x, point.y, point.z});
                        }
                        extrude_window.AddSourceProfile(selected_fill_ids[i], source_points);
                    }
                }
            }
            tool_window.Render(viewport_canvas_pos, viewport_canvas_size);

            const mtcad::SketchPaletteSettings& palette_settings = tool_window.GetSettings();
            viewport_window.SetSketchGridVisible(palette_settings.show_grid);
            viewport_window.SetSnapToGridEnabled(palette_settings.show_snap);

            extrude_window.Render(viewport_canvas_pos, viewport_canvas_size);

            if (extrude_window.IsOpen() && extrude_window.HasPreviewBodies()) {
                std::vector<std::vector<ViewportWindow::Vec3>> body_polygons;
                const auto& preview_polygons = extrude_window.GetPreviewBodyPolygonsWorld();
                body_polygons.reserve(preview_polygons.size());
                for (const auto& preview_points : preview_polygons) {
                    std::vector<ViewportWindow::Vec3> body_points;
                    body_points.reserve(preview_points.size());
                    for (const auto& point : preview_points) {
                        body_points.push_back({point.x, point.y, point.z});
                    }
                    if (body_points.size() >= 3) {
                        body_polygons.push_back(body_points);
                    }
                }
                viewport_window.SetExtrudedBodyPreviewBatch(body_polygons, extrude_window.GetPreviewBodyDepthWorld());
            } else {
                viewport_window.ClearExtrudedBodyPreview();
            }
        }

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
        wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
        wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
        wd->ClearValue.color.float32[3] = clear_color.w;
        if (!is_minimized) {
            mtcad::runtime::FrameRender(wd, draw_data);
            mtcad::runtime::FramePresent(wd);
        }

        if (mtcad::runtime::IsVulkanFatal() && !showed_vulkan_fatal_message) {
            const VkResult last_error = mtcad::runtime::GetVulkanLastError();
            const std::string fatal_msg = std::string("Vulkan fatal error: ") + mtcad::runtime::VkResultName(last_error) +
                " (" + std::to_string((int)last_error) + ")";
            show_fatal_error("MTCAD Vulkan error", fatal_msg.c_str());
            showed_vulkan_fatal_message = true;
            done = true;
        }
    }

    int saved_window_x = 0;
    int saved_window_y = 0;
    int saved_window_w = 0;
    int saved_window_h = 0;
    SDL_GetWindowPosition(window, &saved_window_x, &saved_window_y);
    SDL_GetWindowSize(window, &saved_window_w, &saved_window_h);
    applied_settings.window_x = saved_window_x;
    applied_settings.window_y = saved_window_y;
    if (saved_window_w >= 640) {
        applied_settings.window_width = saved_window_w;
    }
    if (saved_window_h >= 480) {
        applied_settings.window_height = saved_window_h;
    }
    applied_settings.window_fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    mtcad::runtime::SaveUserSettingsIni(user_settings_file.string().c_str(), applied_settings);

    VkResult err = vkDeviceWaitIdle(mtcad::runtime::GetDevice());
    if (err != VK_SUCCESS && err != VK_ERROR_DEVICE_LOST) {
        mtcad::runtime::CheckVkResult(err);
    }

    for (IconSlot& slot : icons) {
        destroy_icon_texture(mtcad::runtime::GetDevice(), mtcad::runtime::GetAllocator(), &slot.texture);
    }


    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    mtcad::runtime::CleanupVulkanWindow(wd);
    mtcad::runtime::CleanupVulkan();

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
