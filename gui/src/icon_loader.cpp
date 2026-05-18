#include "icon_loader.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace {

bool g_recolor_black_pixels = false;
ImVec4 g_black_recolor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

float clamp01(float v) {
    return (std::max)(0.0f, (std::min)(1.0f, v));
}

uint8_t to_u8(float v) {
    return (uint8_t)(clamp01(v) * 255.0f + 0.5f);
}

}  // namespace

void set_icon_loader_black_recolor(const ImVec4* color_rgba) {
    if (color_rgba == nullptr) {
        g_recolor_black_pixels = false;
        g_black_recolor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        return;
    }

    g_recolor_black_pixels = true;
    g_black_recolor = ImVec4(
        clamp01(color_rgba->x),
        clamp01(color_rgba->y),
        clamp01(color_rgba->z),
        clamp01(color_rgba->w));
}

static uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if (((type_filter & (1u << i)) != 0) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool create_buffer(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkAllocationCallbacks* allocator,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer* buffer,
    VkDeviceMemory* memory) {
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult err = vkCreateBuffer(device, &buffer_info, allocator, buffer);
    if (err != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device, *buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(physical_device, mem_requirements.memoryTypeBits, properties);
    if (alloc_info.memoryTypeIndex == UINT32_MAX) {
        return false;
    }

    err = vkAllocateMemory(device, &alloc_info, allocator, memory);
    if (err != VK_SUCCESS) {
        return false;
    }

    err = vkBindBufferMemory(device, *buffer, *memory, 0);
    return err == VK_SUCCESS;
}

static bool create_image_2d_rgba(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkAllocationCallbacks* allocator,
    int width,
    int height,
    VkImage* image,
    VkDeviceMemory* memory) {
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = (uint32_t)width;
    image_info.extent.height = (uint32_t)height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult err = vkCreateImage(device, &image_info, allocator, image);
    if (err != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device, *image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(physical_device, mem_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (alloc_info.memoryTypeIndex == UINT32_MAX) {
        return false;
    }

    err = vkAllocateMemory(device, &alloc_info, allocator, memory);
    if (err != VK_SUCCESS) {
        return false;
    }

    err = vkBindImageMemory(device, *image, *memory, 0);
    return err == VK_SUCCESS;
}

void destroy_icon_texture(VkDevice device, VkAllocationCallbacks* allocator, IconTexture* icon_texture) {
    if (icon_texture == nullptr) {
        return;
    }
    if (icon_texture->descriptor_set != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(icon_texture->descriptor_set);
        icon_texture->descriptor_set = VK_NULL_HANDLE;
    }
    if (icon_texture->sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, icon_texture->sampler, allocator);
        icon_texture->sampler = VK_NULL_HANDLE;
    }
    if (icon_texture->image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, icon_texture->image_view, allocator);
        icon_texture->image_view = VK_NULL_HANDLE;
    }
    if (icon_texture->image != VK_NULL_HANDLE) {
        vkDestroyImage(device, icon_texture->image, allocator);
        icon_texture->image = VK_NULL_HANDLE;
    }
    if (icon_texture->memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, icon_texture->memory, allocator);
        icon_texture->memory = VK_NULL_HANDLE;
    }
    icon_texture->width = 0;
    icon_texture->height = 0;
}

bool load_icon_texture_from_file(
    const char* path,
    VkPhysicalDevice physical_device,
    VkDevice device,
    uint32_t queue_family,
    VkQueue queue,
    VkAllocationCallbacks* allocator,
    IconTexture* icon_texture) {
    if (path == nullptr || icon_texture == nullptr) {
        return false;
    }

    destroy_icon_texture(device, allocator, icon_texture);

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path, &width, &height, &channels, 4);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    if (g_recolor_black_pixels) {
        const uint8_t tint_r = to_u8(g_black_recolor.x);
        const uint8_t tint_g = to_u8(g_black_recolor.y);
        const uint8_t tint_b = to_u8(g_black_recolor.z);
        const float tint_a = clamp01(g_black_recolor.w);
        const int pixel_count = width * height;

        for (int i = 0; i < pixel_count; ++i) {
            stbi_uc* px = pixels + (i * 4);
            if (px[3] == 0) {
                continue;
            }

            // Recolor fully black/near-black pixels while preserving per-pixel alpha edges.
            if (px[0] <= 16 && px[1] <= 16 && px[2] <= 16) {
                px[0] = tint_r;
                px[1] = tint_g;
                px[2] = tint_b;
                px[3] = (stbi_uc)((float)px[3] * tint_a + 0.5f);
            }
        }
    }

    const VkDeviceSize image_size = (VkDeviceSize)width * (VkDeviceSize)height * 4;

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    if (!create_buffer(
            physical_device,
            device,
            allocator,
            image_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &staging_buffer,
            &staging_memory)) {
        stbi_image_free(pixels);
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(device, staging_memory, 0, image_size, 0, &mapped) != VK_SUCCESS) {
        stbi_image_free(pixels);
        return false;
    }
    std::memcpy(mapped, pixels, (size_t)image_size);
    vkUnmapMemory(device, staging_memory);
    stbi_image_free(pixels);

    if (!create_image_2d_rgba(physical_device, device, allocator, width, height, &icon_texture->image, &icon_texture->memory)) {
        return false;
    }

    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = queue_family;
    if (vkCreateCommandPool(device, &pool_info, allocator, &command_pool) != VK_SUCCESS) {
        return false;
    }

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &alloc_info, &command_buffer) != VK_SUCCESS) {
        return false;
    }

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkImageMemoryBarrier barrier_to_transfer = {};
    barrier_to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier_to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_to_transfer.image = icon_texture->image;
    barrier_to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_to_transfer.subresourceRange.baseMipLevel = 0;
    barrier_to_transfer.subresourceRange.levelCount = 1;
    barrier_to_transfer.subresourceRange.baseArrayLayer = 0;
    barrier_to_transfer.subresourceRange.layerCount = 1;
    barrier_to_transfer.srcAccessMask = 0;
    barrier_to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier_to_transfer);

    VkBufferImageCopy copy_region = {};
    copy_region.bufferOffset = 0;
    copy_region.bufferRowLength = 0;
    copy_region.bufferImageHeight = 0;
    copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.mipLevel = 0;
    copy_region.imageSubresource.baseArrayLayer = 0;
    copy_region.imageSubresource.layerCount = 1;
    copy_region.imageOffset = {0, 0, 0};
    copy_region.imageExtent = {(uint32_t)width, (uint32_t)height, 1};
    vkCmdCopyBufferToImage(command_buffer, staging_buffer, icon_texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    VkImageMemoryBarrier barrier_to_shader = {};
    barrier_to_shader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier_to_shader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_to_shader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier_to_shader.image = icon_texture->image;
    barrier_to_shader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier_to_shader.subresourceRange.baseMipLevel = 0;
    barrier_to_shader.subresourceRange.levelCount = 1;
    barrier_to_shader.subresourceRange.baseArrayLayer = 0;
    barrier_to_shader.subresourceRange.layerCount = 1;
    barrier_to_shader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier_to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier_to_shader);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        return false;
    }

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    if (vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
        return false;
    }
    if (vkQueueWaitIdle(queue) != VK_SUCCESS) {
        return false;
    }

    vkDestroyBuffer(device, staging_buffer, allocator);
    vkFreeMemory(device, staging_memory, allocator);
    vkDestroyCommandPool(device, command_pool, allocator);

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = icon_texture->image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &view_info, allocator, &icon_texture->image_view) != VK_SUCCESS) {
        return false;
    }

    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.minLod = -1000.0f;
    sampler_info.maxLod = 1000.0f;
    sampler_info.maxAnisotropy = 1.0f;
    if (vkCreateSampler(device, &sampler_info, allocator, &icon_texture->sampler) != VK_SUCCESS) {
        return false;
    }

    icon_texture->descriptor_set = ImGui_ImplVulkan_AddTexture(icon_texture->sampler, icon_texture->image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (icon_texture->descriptor_set == VK_NULL_HANDLE) {
        return false;
    }

    icon_texture->width = width;
    icon_texture->height = height;
    return true;
}
