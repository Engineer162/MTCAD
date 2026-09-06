#ifndef MTCAD_GUI_APP_VULKAN_RUNTIME_H
#define MTCAD_GUI_APP_VULKAN_RUNTIME_H

#include "imgui.h"
#include "imgui_impl_vulkan.h"

struct SDL_Window;

namespace mtcad {
namespace runtime {

const char* VkResultName(VkResult err);
void CheckVkResult(VkResult err);

void SetupVulkan(ImVector<const char*> instance_extensions);
bool CreateWindowSurface(SDL_Window* window, VkSurfaceKHR* out_surface);
void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height);
void RebuildSwapchainIfNeeded(ImGui_ImplVulkanH_Window* wd, int fb_width, int fb_height);
void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data);
void FramePresent(ImGui_ImplVulkanH_Window* wd);
void CleanupVulkanWindow(ImGui_ImplVulkanH_Window* wd);
void CleanupVulkan();

ImGui_ImplVulkanH_Window* GetMainWindowData();
uint32_t GetMinImageCount();

VkAllocationCallbacks* GetAllocator();
VkInstance GetInstance();
VkPhysicalDevice GetPhysicalDevice();
VkDevice GetDevice();
uint32_t GetQueueFamily();
VkQueue GetQueue();
VkPipelineCache GetPipelineCache();
VkDescriptorPool GetDescriptorPool();

bool IsVulkanFatal();
VkResult GetVulkanLastError();

} // namespace runtime
} // namespace mtcad

#endif
