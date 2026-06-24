#ifndef MTCAD_GUI_ICON_LOADER_H
#define MTCAD_GUI_ICON_LOADER_H

#include "imgui_impl_vulkan.h"

struct IconTexture {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    int width = 0;
    int height = 0;
};

void set_icon_loader_black_recolor(const ImVec4* color_rgba);

bool load_icon_texture_from_file(
    const char* path,
    VkPhysicalDevice physical_device,
    VkDevice device,
    uint32_t queue_family,
    VkQueue queue,
    VkAllocationCallbacks* allocator,
    IconTexture* icon_texture);

void destroy_icon_texture(VkDevice device, VkAllocationCallbacks* allocator, IconTexture* icon_texture);

#endif
