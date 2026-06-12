#include <cstdint>
#include <cstring>
#include <array>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "icon_loader.h"
#include "mtcad/kernel.h"
#include "theme_manager.h"
#include "tools/tools_init.h"
#include "tools/tool_manager.h"
#include "windows/settings_window.h"
#include "windows/extrude_window.h"
#include "windows/tool_window.h"
#include "windows/toolbar_window.h"
#include "windows/viewport_window.h"
#include "windows/workspace_browser_window.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// Windows specific macros
#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#ifndef MTCAD_PREFER_HIGH_PERFORMANCE_GPU
#define MTCAD_PREFER_HIGH_PERFORMANCE_GPU 1
#endif
#endif

#if defined(_WIN32) && (MTCAD_PREFER_HIGH_PERFORMANCE_GPU != 0)
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0x00000001;
}
#endif

static VkAllocationCallbacks* g_allocator = nullptr;
static VkInstance g_instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_physical_device = VK_NULL_HANDLE;
static VkDevice g_device = VK_NULL_HANDLE;
static uint32_t g_queue_family = (uint32_t)-1;
static VkQueue g_queue = VK_NULL_HANDLE;
static VkPipelineCache g_pipeline_cache = VK_NULL_HANDLE;
static VkDescriptorPool g_descriptor_pool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_main_window_data;
static uint32_t g_min_image_count = 2;
static bool g_swapchain_rebuild = false;
static bool g_vulkan_fatal = false;
static VkResult g_vulkan_last_error = VK_SUCCESS;

static float clampf(float v, float min_v, float max_v) {
    if (v < min_v) {
        return min_v;
    }
    if (v > max_v) {
        return max_v;
    }
    return v;
}

static std::string trim_copy(const std::string& value) {
    const std::string ws = " \t\r\n";
    const size_t start = value.find_first_not_of(ws);
    if (start == std::string::npos) {
        return std::string();
    }
    const size_t end = value.find_last_not_of(ws);
    return value.substr(start, end - start + 1);
}

static std::filesystem::path get_settings_directory()
{
#if defined(_WIN32)

    PWSTR path = nullptr;

    HRESULT hr = SHGetKnownFolderPath(
        FOLDERID_RoamingAppData,
        0,
        nullptr,
        &path);

    if (SUCCEEDED(hr) && path != nullptr)
    {
        std::filesystem::path result(path);
        CoTaskMemFree(path);

        result /= "MTCAD";

        std::error_code ec;
        std::filesystem::create_directories(result, ec);

        return result;
    }

#endif

    // Fallback to binary directory if we cant get an appdata path.
    return std::filesystem::current_path();
}

static bool load_user_settings_ini(const char* file_path, UserSettings* out_settings) {
    if (file_path == nullptr || out_settings == nullptr) {
        return false;
    }
    std::ifstream in(file_path);
    if (!in.is_open()) {
        return false;
    }

    UserSettings loaded = *out_settings;
    std::string line;
    while (std::getline(in, line)) {
        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = trim_copy(line.substr(0, equals));
        const std::string value = trim_copy(line.substr(equals + 1));
        if (key.empty() || value.empty()) {
            continue;
        }

        try {
            if (key == "text_scale") {
                loaded.text_scale = std::stof(value);
            } else if (key == "icon_scale") {
                loaded.icon_scale = std::stof(value);
            } else if (key == "theme_index") {
                loaded.theme_index = std::stoi(value);
            } else if (key == "viewport_drag_button") {
                loaded.viewport_drag_button = std::stoi(value);
            } else if (key == "viewport_orbit_button") {
                loaded.viewport_orbit_button = std::stoi(value);
            } else if (key == "keyboard_navigation") {
                loaded.keyboard_navigation_enabled = (std::stoi(value) != 0);
            } else if (key == "workspace_root") {
                loaded.workspace_root = value;
            } else if (key == "window_x") {
                loaded.window_x = std::stoi(value);
            } else if (key == "window_y") {
                loaded.window_y = std::stoi(value);
            } else if (key == "window_width") {
                loaded.window_width = std::stoi(value);
            } else if (key == "window_height") {
                loaded.window_height = std::stoi(value);
            } else if (key == "window_fullscreen") {
                loaded.window_fullscreen = (std::stoi(value) != 0);
            }
        } catch (...) {
            continue;
        }
    }

    loaded.text_scale = clampf(loaded.text_scale, 0.80f, 2.00f);
    loaded.icon_scale = clampf(loaded.icon_scale, 0.80f, 2.00f);
    loaded.theme_index = clamp_theme_index(loaded.theme_index);
    if (loaded.viewport_drag_button < 0 || loaded.viewport_drag_button > 2) {
        loaded.viewport_drag_button = 1;
    }
    if (loaded.viewport_orbit_button < 0 || loaded.viewport_orbit_button > 2) {
        loaded.viewport_orbit_button = 1;
    }
    if (loaded.window_width < 640) {
        loaded.window_width = 640;
    }
    if (loaded.window_height < 480) {
        loaded.window_height = 480;
    }
    if (loaded.workspace_root.empty()) {
        loaded.workspace_root = std::filesystem::current_path().string();
    }

    *out_settings = loaded;
    return true;
}

static bool save_user_settings_ini(const char* file_path, const UserSettings& settings) {
    if (file_path == nullptr) {
        return false;
    }
    std::ofstream out(file_path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "[MTCAD]\n";
    out << "text_scale=" << settings.text_scale << "\n";
    out << "icon_scale=" << settings.icon_scale << "\n";
    out << "theme_index=" << clamp_theme_index(settings.theme_index) << "\n";
    out << "viewport_drag_button=" << settings.viewport_drag_button << "\n";
    out << "viewport_orbit_button=" << settings.viewport_orbit_button << "\n";
    out << "keyboard_navigation=" << (settings.keyboard_navigation_enabled ? 1 : 0) << "\n";
    out << "workspace_root=" << settings.workspace_root << "\n";
    out << "window_x=" << settings.window_x << "\n";
    out << "window_y=" << settings.window_y << "\n";
    out << "window_width=" << settings.window_width << "\n";
    out << "window_height=" << settings.window_height << "\n";
    out << "window_fullscreen=" << (settings.window_fullscreen ? 1 : 0) << "\n";
    return out.good();
}

static ImGuiMouseButton drag_button_from_index(int index) {
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

static std::string resolve_icon_path(const char* icon_name) {
    if (icon_name == nullptr || icon_name[0] == '\0') {
        return std::string();
    }

    const std::filesystem::path icon_path = std::filesystem::path("assets") / "icons" / (std::string(icon_name) + ".png");
    if (std::filesystem::exists(icon_path)) {
        return icon_path.string();
    }
    return std::string();
}

enum class AppIcon {
    Settings = 0,
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

static const char* vk_result_name(VkResult err) {
    switch (err) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        default: return "VK_RESULT_UNKNOWN";
    }
}

static void check_vk_result(VkResult err) {
    if (err == VK_SUCCESS) {
        return;
    }
    std::fprintf(stderr, "[vulkan] Error: %s (%d)\n", vk_result_name(err), err);
    if (err < 0) {
        g_vulkan_fatal = true;
        g_vulkan_last_error = err;
    }
}

static bool is_extension_available(const ImVector<VkExtensionProperties>& properties, const char* extension) {
    for (const VkExtensionProperties& p : properties) {
        if (std::strcmp(p.extensionName, extension) == 0) {
            return true;
        }
    }
    return false;
}

static void setup_vulkan(ImVector<const char*> instance_extensions) {
    VkResult err;

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "MTCAD";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "MTCAD";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    uint32_t properties_count = 0;
    ImVector<VkExtensionProperties> properties;
    vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
    properties.resize(properties_count);
    err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
    check_vk_result(err);

    if (is_extension_available(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
        instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
    create_info.ppEnabledExtensionNames = instance_extensions.Data;
    err = vkCreateInstance(&create_info, g_allocator, &g_instance);
    check_vk_result(err);

    uint32_t gpu_count = 0;
    err = vkEnumeratePhysicalDevices(g_instance, &gpu_count, nullptr);
    check_vk_result(err);
    if (gpu_count == 0) {
        g_vulkan_fatal = true;
        g_vulkan_last_error = VK_ERROR_INITIALIZATION_FAILED;
        return;
    }

    ImVector<VkPhysicalDevice> physical_devices;
    physical_devices.resize((int)gpu_count);
    err = vkEnumeratePhysicalDevices(g_instance, &gpu_count, physical_devices.Data);
    check_vk_result(err);
    if (g_vulkan_fatal) {
        return;
    }
    g_physical_device = physical_devices[0];

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_family_count, nullptr);
    ImVector<VkQueueFamilyProperties> queue_families;
    queue_families.resize((int)queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_physical_device, &queue_family_count, queue_families.Data);

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if ((queue_families[(int)i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            g_queue_family = i;
            break;
        }
    }
    if (g_queue_family == (uint32_t)-1) {
        g_vulkan_fatal = true;
        g_vulkan_last_error = VK_ERROR_INITIALIZATION_FAILED;
        return;
    }

    ImVector<const char*> device_extensions;
    device_extensions.push_back("VK_KHR_swapchain");

    const float queue_priority[] = { 1.0f };
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = g_queue_family;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = queue_info;
    device_create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
    device_create_info.ppEnabledExtensionNames = device_extensions.Data;

    err = vkCreateDevice(g_physical_device, &device_create_info, g_allocator, &g_device);
    check_vk_result(err);
    vkGetDeviceQueue(g_device, g_queue_family, 0, &g_queue);

    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    err = vkCreateDescriptorPool(g_device, &pool_info, g_allocator, &g_descriptor_pool);
    check_vk_result(err);
}

static void setup_vulkan_window(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height) {
    VkBool32 res = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_physical_device, g_queue_family, surface, &res);
    if (res != VK_TRUE) {
        std::fprintf(stderr, "Error: no WSI support on selected device\n");
        std::exit(1);
    }

    const VkFormat request_surface_image_format[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_R8G8B8_UNORM,
    };
    const VkColorSpaceKHR request_surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->Surface = surface;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        g_physical_device,
        wd->Surface,
        request_surface_image_format,
        IM_ARRAYSIZE(request_surface_image_format),
        request_surface_color_space
    );

    const VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR };
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_physical_device, wd->Surface, present_modes, IM_ARRAYSIZE(present_modes));

    IM_ASSERT(g_min_image_count >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(
        g_instance,
        g_physical_device,
        g_device,
        wd,
        g_queue_family,
        g_allocator,
        width,
        height,
        g_min_image_count,
        0
    );
}

static void cleanup_vulkan_window(ImGui_ImplVulkanH_Window* wd) {
    ImGui_ImplVulkanH_DestroyWindow(g_instance, g_device, wd, g_allocator);
    vkDestroySurfaceKHR(g_instance, wd->Surface, g_allocator);
}

static void cleanup_vulkan() {
    vkDestroyDescriptorPool(g_device, g_descriptor_pool, g_allocator);
    vkDestroyDevice(g_device, g_allocator);
    vkDestroyInstance(g_instance, g_allocator);
}

static void frame_render(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data) {
    if (g_vulkan_fatal) {
        return;
    }

    VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

    VkResult err = vkAcquireNextImageKHR(g_device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        g_swapchain_rebuild = true;
    }
    if (err == VK_ERROR_OUT_OF_DATE_KHR) {
        return;
    }
    if (err != VK_SUCCESS && err != VK_SUBOPTIMAL_KHR) {
        check_vk_result(err);
        return;
    }

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    err = vkWaitForFences(g_device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
    check_vk_result(err);
    if (g_vulkan_fatal) {
        return;
    }

    err = vkResetFences(g_device, 1, &fd->Fence);
    check_vk_result(err);
    if (g_vulkan_fatal) {
        return;
    }

    err = vkResetCommandPool(g_device, fd->CommandPool, 0);
    check_vk_result(err);
    if (g_vulkan_fatal) {
        return;
    }

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &begin_info);
    check_vk_result(err);
    if (g_vulkan_fatal) {
        return;
    }

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = wd->RenderPass;
    render_pass_info.framebuffer = fd->Framebuffer;
    render_pass_info.renderArea.extent.width = wd->Width;
    render_pass_info.renderArea.extent.height = wd->Height;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &wd->ClearValue;

    vkCmdBeginRenderPass(fd->CommandBuffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);
    vkCmdEndRenderPass(fd->CommandBuffer);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_acquired_semaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &fd->CommandBuffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_complete_semaphore;

    err = vkEndCommandBuffer(fd->CommandBuffer);
    check_vk_result(err);
    if (g_vulkan_fatal) {
        return;
    }

    err = vkQueueSubmit(g_queue, 1, &submit_info, fd->Fence);
    check_vk_result(err);
}

static void frame_present(ImGui_ImplVulkanH_Window* wd) {
    if (g_swapchain_rebuild || g_vulkan_fatal) {
        return;
    }

    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;

    VkResult err = vkQueuePresentKHR(g_queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        g_swapchain_rebuild = true;
    }
    if (err == VK_ERROR_OUT_OF_DATE_KHR) {
        return;
    }
    if (err != VK_SUCCESS && err != VK_SUBOPTIMAL_KHR) {
        check_vk_result(err);
        return;
    }

    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
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

    initialize_theme_manager("assets/themes");
    UserSettings applied_settings;
    applied_settings.window_x = SDL_WINDOWPOS_CENTERED;
    applied_settings.window_y = SDL_WINDOWPOS_CENTERED;
    load_user_settings_ini(user_settings_file.string().c_str(), &applied_settings);

    const mtcad_kernel_version version = mtcad_kernel_get_version();

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

    setup_vulkan(instance_extensions);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, g_instance, g_allocator, &surface)) {
        show_fatal_error("MTCAD startup error", "Failed to create Vulkan surface.");
        SDL_DestroyWindow(window);
        cleanup_vulkan();
        SDL_Quit();
        return 1;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    ImGui_ImplVulkanH_Window* wd = &g_main_window_data;
    setup_vulkan_window(wd, surface, width, height);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    static std::string imgui_ini_path =
    (settings_dir / "imgui.ini").string();
    io.IniFilename = imgui_ini_path.c_str();

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    apply_imgui_theme(applied_settings.theme_index);

    ImGui_ImplSDL3_InitForVulkan(window);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_instance;
    init_info.PhysicalDevice = g_physical_device;
    init_info.Device = g_device;
    init_info.QueueFamily = g_queue_family;
    init_info.Queue = g_queue;
    init_info.PipelineCache = g_pipeline_cache;
    init_info.DescriptorPool = g_descriptor_pool;
    init_info.MinImageCount = g_min_image_count;
    init_info.ImageCount = wd->ImageCount;
    init_info.Allocator = g_allocator;
    init_info.ApiVersion = VK_API_VERSION_1_1;
    init_info.MinAllocationSize = 1024 * 1024;
    init_info.PipelineInfoMain.RenderPass = wd->RenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    ImVec4 selected_theme_icon_tint;
    if (get_theme_icon_tint(applied_settings.theme_index, &selected_theme_icon_tint)) {
        set_icon_loader_black_recolor(&selected_theme_icon_tint);
    } else {
        set_icon_loader_black_recolor(nullptr);
    }

    std::array<IconSlot, k_app_icon_count> icons = {{
        {"settings"},
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
        slot.path = resolve_icon_path(slot.name);
        if (!slot.path.empty()) {
            slot.loaded = load_icon_texture_from_file(
                slot.path.c_str(),
                g_physical_device,
                g_device,
                g_queue_family,
                g_queue,
                g_allocator,
                &slot.texture);
        }
    }

    ImVec4 clear_color(0.10f, 0.11f, 0.13f, 1.0f);

    mtcad::InitializeTools();

    WorkspaceBrowserWindow workspace_browser_window;
    ToolbarWindow toolbar_window;
    ViewportWindow viewport_window;
    SettingsWindow settings_window;
    mtcad::SketchPaletteWindow tool_window;
    mtcad::ExtrudePaletteWindow extrude_window;

    auto reload_single_icon = [&](IconSlot* slot) {
        if (slot == nullptr) {
            return;
        }
        if (slot->path.empty()) {
            destroy_icon_texture(g_device, g_allocator, &slot->texture);
            slot->loaded = false;
            return;
        }
        slot->loaded = load_icon_texture_from_file(
            slot->path.c_str(),
            g_physical_device,
            g_device,
            g_queue_family,
            g_queue,
            g_allocator,
            &slot->texture);
    };

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

    auto reload_icons_for_theme = [&](int theme_index) {
        VkResult idle_err = vkDeviceWaitIdle(g_device);
        if (idle_err != VK_SUCCESS && idle_err != VK_ERROR_DEVICE_LOST) {
            check_vk_result(idle_err);
            return;
        }

        ImVec4 icon_tint;
        if (get_theme_icon_tint(theme_index, &icon_tint)) {
            set_icon_loader_black_recolor(&icon_tint);
        } else {
            set_icon_loader_black_recolor(nullptr);
        }

        for (IconSlot& slot : icons) {
            reload_single_icon(&slot);
        }

        apply_loaded_icon_textures();
    };

    float ui_text_scale = applied_settings.text_scale;
    float ui_icon_scale = applied_settings.icon_scale;
    viewport_window.SetDragButton(drag_button_from_index(applied_settings.viewport_drag_button));
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

    bool show_settings_window = false;

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
        if (fb_width > 0 && fb_height > 0 && (g_swapchain_rebuild || wd->Width != fb_width || wd->Height != fb_height)) {
            ImGui_ImplVulkan_SetMinImageCount(g_min_image_count);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_instance, g_physical_device, g_device, wd, g_queue_family, g_allocator, fb_width, fb_height, g_min_image_count, 0);
            wd->FrameIndex = 0;
            g_swapchain_rebuild = false;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        io.FontGlobalScale = ui_text_scale;

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(main_viewport->WorkPos);
        ImGui::SetNextWindowSize(main_viewport->WorkSize);
        ImGui::SetNextWindowViewport(main_viewport->ID);

        const ImVec2 settings_button_padding(4.0f, 3.0f);
        const float requested_icon_size = 16.0f * ui_icon_scale;
        const float min_button_height_for_icon = requested_icon_size + settings_button_padding.y * 2.0f;
        const float base_menu_frame_padding_y = 6.0f * ui_text_scale;
        float workspace_menu_frame_padding_y = base_menu_frame_padding_y;
        const float required_menu_frame_padding_y = (min_button_height_for_icon - ImGui::GetFontSize()) * 0.5f;
        if (workspace_menu_frame_padding_y < required_menu_frame_padding_y) {
            workspace_menu_frame_padding_y = required_menu_frame_padding_y;
        }
        if (workspace_menu_frame_padding_y < 4.0f) {
            workspace_menu_frame_padding_y = 4.0f;
        }
        const ImVec2 workspace_menu_frame_padding(8.0f * ui_text_scale, workspace_menu_frame_padding_y);

        ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
        host_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        host_flags |= ImGuiWindowFlags_MenuBar;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, workspace_menu_frame_padding);
        ImGui::Begin("MTCAD Workspace", nullptr, host_flags);
        ImGuiID dockspace_id = ImGui::GetID("MTCAD_Dockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        if (ImGui::BeginMenuBar()) {
            float settings_button_height = ImGui::GetFrameHeight();
            if (settings_button_height < 18.0f) {
                settings_button_height = 18.0f;
            }
            const float max_icon_size = settings_button_height - settings_button_padding.y * 2.0f;
            float settings_icon_size = requested_icon_size;
            if (settings_icon_size > max_icon_size) {
                settings_icon_size = max_icon_size;
            }
            if (settings_icon_size < 12.0f) {
                settings_icon_size = 12.0f;
            }

            ImGui::Text("Kernel %d.%d.%d", version.major, version.minor, version.patch);

            const char* fallback_label = "Cfg";
            const float fallback_width = ImGui::CalcTextSize(fallback_label).x + settings_button_padding.x * 2.0f;
            const float icon_button_width = settings_icon_size + settings_button_padding.x * 2.0f;
            const IconSlot& settings_icon = icon_slot(AppIcon::Settings);
            const bool has_settings_icon = settings_icon.loaded && settings_icon.texture.descriptor_set != VK_NULL_HANDLE;
            const float button_width = has_settings_icon ? icon_button_width : fallback_width;
            const float right_padding = ImGui::GetStyle().FramePadding.x;
            const float settings_x = ImGui::GetWindowContentRegionMax().x - button_width - right_padding;
            if (ImGui::GetCursorPosX() < settings_x) {
                ImGui::SetCursorPosX(settings_x);
            }

            if (has_settings_icon) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, settings_button_padding);
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(49, 67, 90, 230));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(67, 92, 124, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(80, 108, 145, 255));
                if (ImGui::ImageButton("##settings_icon", (ImTextureID)settings_icon.texture.descriptor_set, ImVec2(settings_icon_size, settings_icon_size))) {
                    show_settings_window = true;
                }
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Settings");
                }
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, settings_button_padding);
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(49, 67, 90, 230));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(67, 92, 124, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(80, 108, 145, 255));
                if (ImGui::Button(fallback_label, ImVec2(0.0f, settings_button_height))) {
                    show_settings_window = true;
                }
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Settings");
                }
            }
            ImGui::EndMenuBar();
        }
        ImGui::End();
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
                if (pending_settings.viewport_drag_button < 0 || pending_settings.viewport_drag_button > 2) {
                    pending_settings.viewport_drag_button = 1;
                }
                if (pending_settings.viewport_orbit_button < 0 || pending_settings.viewport_orbit_button > 2) {
                    pending_settings.viewport_orbit_button = 1;
                }

                applied_settings = pending_settings;
                ui_text_scale = applied_settings.text_scale;
                ui_icon_scale = applied_settings.icon_scale;
                apply_imgui_theme(applied_settings.theme_index);
                viewport_window.SetDragButton(drag_button_from_index(applied_settings.viewport_drag_button));
                viewport_window.SetOrbitButton(orbit_button_from_index(applied_settings.viewport_orbit_button));
                workspace_browser_window.SetRootDirectory(applied_settings.workspace_root);

                if (applied_settings.keyboard_navigation_enabled) {
                    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                } else {
                    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
                }

                save_user_settings_ini(user_settings_file.string().c_str(), applied_settings);
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
            reload_icons_for_theme(desired_icon_theme_index);
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
            viewport_window.ClearExtrudedBodyPreview();
            viewport_window.ClearExtrudedBodyFinal();
            if (extrude_window.HasPreviewBody()) {
                std::vector<ViewportWindow::Vec3> body_points;
                const std::vector<mtcad::ExtrudePoint3D>& preview_points = extrude_window.GetPreviewBodyPolygonWorld();
                body_points.reserve(preview_points.size());
                for (const auto& point : preview_points) {
                    body_points.push_back({point.x, point.y, point.z});
                }
                viewport_window.SetExtrudedBodyFinal(
                    body_points,
                    extrude_window.GetPreviewBodyDepthWorld()
                );
            }
            extrude_window.Close();
        }

        if (extrude_window.ConsumeCancelExtrude()) {
            extrude_window.Close();
        }

        ImVec2 viewport_canvas_pos;
        ImVec2 viewport_canvas_size;
        if (viewport_window.GetCanvasRect(&viewport_canvas_pos, &viewport_canvas_size)) {
            std::vector<ViewportWindow::Vec3> selected_polygon_world_points;
            if (viewport_window.GetSelectedPolygonWorldPoints(&selected_polygon_world_points)) {
                int selected_polygon_index = -1;
                int selected_overlap_index = -1;
                if (viewport_window.GetSelectedFillIds(&selected_polygon_index, &selected_overlap_index)) {
                    const int selected_fill_id = (selected_overlap_index != -1) ? selected_overlap_index : selected_polygon_index;
                    extrude_window.SetSourceFillId(selected_fill_id);
                } else {
                    extrude_window.SetSourceFillId(-1);
                }
                std::vector<mtcad::ExtrudePoint3D> source_points;
                source_points.reserve(selected_polygon_world_points.size());
                for (const auto& point : selected_polygon_world_points) {
                    source_points.push_back({point.x, point.y, point.z});
                }
                extrude_window.SetSourcePolygonWorld(source_points);
            } else {
                extrude_window.SetSourceFillId(-1);
            }
            tool_window.Render(viewport_canvas_pos, viewport_canvas_size);
            extrude_window.Render(viewport_canvas_pos, viewport_canvas_size);

            if (extrude_window.IsOpen() && extrude_window.HasPreviewBody()) {
                std::vector<ViewportWindow::Vec3> body_points;
                const std::vector<mtcad::ExtrudePoint3D>& preview_points = extrude_window.GetPreviewBodyPolygonWorld();
                body_points.reserve(preview_points.size());
                for (const auto& point : preview_points) {
                    body_points.push_back({point.x, point.y, point.z});
                }
                viewport_window.SetExtrudedBodyPreview(body_points, extrude_window.GetPreviewBodyDepthWorld());
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
            frame_render(wd, draw_data);
            frame_present(wd);
        }

        if (g_vulkan_fatal && !showed_vulkan_fatal_message) {
            const std::string fatal_msg = std::string("Vulkan fatal error: ") + vk_result_name(g_vulkan_last_error) +
                " (" + std::to_string((int)g_vulkan_last_error) + ")";
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
    save_user_settings_ini(user_settings_file.string().c_str(), applied_settings);

    VkResult err = vkDeviceWaitIdle(g_device);
    if (err != VK_SUCCESS && err != VK_ERROR_DEVICE_LOST) {
        check_vk_result(err);
    }

    for (IconSlot& slot : icons) {
        destroy_icon_texture(g_device, g_allocator, &slot.texture);
    }


    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    cleanup_vulkan_window(wd);
    cleanup_vulkan();

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
