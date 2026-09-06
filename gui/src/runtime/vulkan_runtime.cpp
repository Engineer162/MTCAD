#include "vulkan_runtime.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "../version.h"
#include "mtkernel/kernel.h"

namespace {

VkAllocationCallbacks* g_allocator = nullptr;
VkInstance g_instance = VK_NULL_HANDLE;
VkPhysicalDevice g_physical_device = VK_NULL_HANDLE;
VkDevice g_device = VK_NULL_HANDLE;
uint32_t g_queue_family = (uint32_t)-1;
VkQueue g_queue = VK_NULL_HANDLE;
VkPipelineCache g_pipeline_cache = VK_NULL_HANDLE;
VkDescriptorPool g_descriptor_pool = VK_NULL_HANDLE;

ImGui_ImplVulkanH_Window g_main_window_data;
uint32_t g_min_image_count = 2;
bool g_swapchain_rebuild = false;
bool g_vulkan_fatal = false;
VkResult g_vulkan_last_error = VK_SUCCESS;

bool is_extension_available(const ImVector<VkExtensionProperties>& properties, const char* extension) {
    for (const VkExtensionProperties& p : properties) {
        if (std::strcmp(p.extensionName, extension) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

namespace mtcad {
namespace runtime {

const char* VkResultName(VkResult err) {
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

void CheckVkResult(VkResult err) {
    if (err == VK_SUCCESS) {
        return;
    }
    std::fprintf(stderr, "[vulkan] Error: %s (%d)\n", VkResultName(err), err);
    if (err < 0) {
        g_vulkan_fatal = true;
        g_vulkan_last_error = err;
    }
}

void SetupVulkan(ImVector<const char*> instance_extensions) {
    VkResult err;

    const mtcad_application_version applicationVer = mtcad_application_get_version();
    const mtkernel_version kernel_ver = mtkernel_get_version();

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "MTCAD";
    app_info.applicationVersion = VK_MAKE_VERSION(applicationVer.major, applicationVer.minor, applicationVer.patch);
    app_info.pEngineName = "MTKernel";
    app_info.engineVersion = VK_MAKE_API_VERSION(0, kernel_ver.major, kernel_ver.minor, kernel_ver.patch);
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    uint32_t properties_count = 0;
    ImVector<VkExtensionProperties> properties;
    vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
    properties.resize(properties_count);
    err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
    CheckVkResult(err);

    if (is_extension_available(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
        instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
    create_info.ppEnabledExtensionNames = instance_extensions.Data;
    err = vkCreateInstance(&create_info, g_allocator, &g_instance);
    CheckVkResult(err);

    uint32_t gpu_count = 0;
    err = vkEnumeratePhysicalDevices(g_instance, &gpu_count, nullptr);
    CheckVkResult(err);
    if (gpu_count == 0) {
        g_vulkan_fatal = true;
        g_vulkan_last_error = VK_ERROR_INITIALIZATION_FAILED;
        return;
    }

    ImVector<VkPhysicalDevice> physical_devices;
    physical_devices.resize((int)gpu_count);
    err = vkEnumeratePhysicalDevices(g_instance, &gpu_count, physical_devices.Data);
    CheckVkResult(err);
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
    CheckVkResult(err);
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
    CheckVkResult(err);
}

bool CreateWindowSurface(SDL_Window* window, VkSurfaceKHR* out_surface) {
    if (window == nullptr || out_surface == nullptr) {
        return false;
    }
    *out_surface = VK_NULL_HANDLE;
    return SDL_Vulkan_CreateSurface(window, g_instance, g_allocator, out_surface);
}

void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height) {
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

void RebuildSwapchainIfNeeded(ImGui_ImplVulkanH_Window* wd, int fb_width, int fb_height) {
    if (fb_width <= 0 || fb_height <= 0) {
        return;
    }

    if (g_swapchain_rebuild || wd->Width != fb_width || wd->Height != fb_height) {
        ImGui_ImplVulkan_SetMinImageCount(g_min_image_count);
        ImGui_ImplVulkanH_CreateOrResizeWindow(g_instance, g_physical_device, g_device, wd, g_queue_family, g_allocator, fb_width, fb_height, g_min_image_count, 0);
        wd->FrameIndex = 0;
        g_swapchain_rebuild = false;
    }
}

void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data) {
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
        CheckVkResult(err);
        return;
    }

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    err = vkWaitForFences(g_device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
    CheckVkResult(err);
    if (g_vulkan_fatal) {
        return;
    }

    err = vkResetFences(g_device, 1, &fd->Fence);
    CheckVkResult(err);
    if (g_vulkan_fatal) {
        return;
    }

    err = vkResetCommandPool(g_device, fd->CommandPool, 0);
    CheckVkResult(err);
    if (g_vulkan_fatal) {
        return;
    }

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &begin_info);
    CheckVkResult(err);
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
    CheckVkResult(err);
    if (g_vulkan_fatal) {
        return;
    }

    err = vkQueueSubmit(g_queue, 1, &submit_info, fd->Fence);
    CheckVkResult(err);
}

void FramePresent(ImGui_ImplVulkanH_Window* wd) {
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
        CheckVkResult(err);
        return;
    }

    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
}

void CleanupVulkanWindow(ImGui_ImplVulkanH_Window* wd) {
    ImGui_ImplVulkanH_DestroyWindow(g_instance, g_device, wd, g_allocator);
    vkDestroySurfaceKHR(g_instance, wd->Surface, g_allocator);
}

void CleanupVulkan() {
    vkDestroyDescriptorPool(g_device, g_descriptor_pool, g_allocator);
    vkDestroyDevice(g_device, g_allocator);
    vkDestroyInstance(g_instance, g_allocator);
}

ImGui_ImplVulkanH_Window* GetMainWindowData() {
    return &g_main_window_data;
}

uint32_t GetMinImageCount() {
    return g_min_image_count;
}

VkAllocationCallbacks* GetAllocator() {
    return g_allocator;
}

VkInstance GetInstance() {
    return g_instance;
}

VkPhysicalDevice GetPhysicalDevice() {
    return g_physical_device;
}

VkDevice GetDevice() {
    return g_device;
}

uint32_t GetQueueFamily() {
    return g_queue_family;
}

VkQueue GetQueue() {
    return g_queue;
}

VkPipelineCache GetPipelineCache() {
    return g_pipeline_cache;
}

VkDescriptorPool GetDescriptorPool() {
    return g_descriptor_pool;
}

bool IsVulkanFatal() {
    return g_vulkan_fatal;
}

VkResult GetVulkanLastError() {
    return g_vulkan_last_error;
}

} // namespace runtime
} // namespace mtcad
