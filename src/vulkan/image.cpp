#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#include <vk_mem_alloc.h>

#include <spdlog/spdlog.h>

#include "allocator.h"
#include "image.h"
#include "utils.h"

Image::Image(vk::ImageCreateInfo create_info, const Allocator& allocator,
             const VmaAllocationCreateFlags allocation_create_flags) : vma_allocator(allocator.get()) {
    SCOPED_TIMER_NAMED("Create VkImage");

    const VmaAllocationCreateInfo allocation_create_info = {
        .flags = allocation_create_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VkImage image;
    const auto result = vmaCreateImage(allocator.get(),
                                       create_info,
                                       &allocation_create_info,
                                       &image,
                                       &vma_allocation,
                                       nullptr);

    if (result != VK_SUCCESS) {
        spdlog::error("vmaCreateImage failed: {}", vk::to_string(static_cast<vk::Result>(result)));
    }
    handle = image;
}

Image::~Image() {
    vmaDestroyImage(vma_allocator, handle, vma_allocation);
}

Image::Image(Image&& other) noexcept
    : handle(other.handle),
      vma_allocation(other.vma_allocation),
      vma_allocator(other.vma_allocator) {
    other.handle = nullptr;
    other.vma_allocation = nullptr;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (handle) {
        vmaDestroyImage(vma_allocator, handle, vma_allocation);
    }

    handle = std::exchange(other.handle, nullptr);
    vma_allocation = std::exchange(other.vma_allocation, nullptr);
    vma_allocator = other.vma_allocator;

    return *this;
}

const vk::Image& Image::get() const {
    return handle;
}

VmaAllocation Image::get_allocation() const {
    return vma_allocation;
}