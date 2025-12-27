#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#include <vk_mem_alloc.h>

#include <spdlog/spdlog.h>

#include "allocator.h"
#include "buffer.h"
#include "utils.h"

Buffer::Buffer(const Allocator& allocator, const vk::DeviceSize size, const vk::BufferUsageFlags usage,
               const VmaAllocationCreateFlags allocation_create_flags) : vma_allocator(allocator.get()) {
    SCOPED_TIMER_NAMED("Create VkBuffer");

    const vk::BufferCreateInfo buffer_create_info = {
        .size = size,
        .usage = usage,
    };

    const VmaAllocationCreateInfo allocation_create_info = {
        .flags = allocation_create_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    VkBuffer buffer;
    const auto result = vmaCreateBuffer(allocator.get(),
                                        buffer_create_info,
                                        &allocation_create_info,
                                        &buffer,
                                        &vma_allocation,
                                        nullptr);

    if (result != VK_SUCCESS) {
        spdlog::error("vmaCreateBuffer failed: {}", vk::to_string(static_cast<vk::Result>(result)));
    }
    handle = buffer;
}

Buffer::~Buffer() {
    vmaDestroyBuffer(vma_allocator, this->handle, vma_allocation);
}

Buffer::Buffer(Buffer&& other) noexcept
    : handle(other.handle),
      vma_allocation(other.vma_allocation),
      vma_allocator(other.vma_allocator) {
    other.handle = nullptr;
    other.vma_allocation = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (handle) {
        vmaDestroyBuffer(vma_allocator, handle, vma_allocation);
    }

    handle = std::exchange(other.handle, nullptr);
    vma_allocation = std::exchange(other.vma_allocation, nullptr);
    vma_allocator = other.vma_allocator;

    return *this;
}

const vk::Buffer& Buffer::get() const {
    return handle;
}

VmaAllocation Buffer::get_allocation() const {
    return vma_allocation;
}