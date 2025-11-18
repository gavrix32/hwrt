#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#include <vk_mem_alloc.h>

#include <spdlog/spdlog.h>

#include "allocator.h"
#include "image.h"
#include "utils.h"

Image::Image(vk::ImageCreateInfo image_create_info, const Allocator& allocator,
             const VmaAllocationCreateFlags allocation_create_flags) : vma_allocator(allocator.get()) {
    SCOPED_TIMER_NAMED("Create VkImage");

    const VmaAllocationCreateInfo allocation_create_info = {
        .flags = allocation_create_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VkImage image;
    const auto result = vmaCreateImage(allocator.get(),
                                       image_create_info,
                                       &allocation_create_info,
                                       &image,
                                       &vma_allocation,
                                       nullptr);

    if (result != VK_SUCCESS) {
        spdlog::error("vmaCreateImage failed: {}", vk::to_string(static_cast<vk::Result>(result)));
    }
    vk_image = image;
}

Image::~Image() {
    vmaDestroyImage(vma_allocator, vk_image, vma_allocation);
}

const vk::Image& Image::get() const {
    return vk_image;
}

VmaAllocation Image::get_allocation() const {
    return vma_allocation;
}