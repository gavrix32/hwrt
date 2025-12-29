#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#include <spdlog/spdlog.h>

#include "allocator.h"
#include "buffer.h"

#include "device.h"
#include "utils.h"

Buffer::Buffer(const Allocator& allocator,
               const vk::DeviceSize size,
               const vk::BufferUsageFlags usage,
               const VmaMemoryUsage memory_usage,
               const VmaAllocationCreateFlags allocation_flags,
               const uint32_t min_alignment)
    : allocator(allocator.get()) {
    SCOPED_TIMER();

    const vk::BufferCreateInfo buffer_create_info = {
        .size = size,
        .usage = usage,
    };

    const VmaAllocationCreateInfo alloc_create_info = {
        .flags = allocation_flags,
        .usage = memory_usage,
    };

    VkBuffer buffer;
    VmaAllocationInfo alloc_info;

    VkResult result;

    if (min_alignment != 0) {
        result = vmaCreateBufferWithAlignment(allocator.get(),
                                              reinterpret_cast<const VkBufferCreateInfo*>(&buffer_create_info),
                                              &alloc_create_info,
                                              min_alignment,
                                              &buffer,
                                              &allocation,
                                              &alloc_info);
    } else {
        result = vmaCreateBuffer(allocator.get(),
                                 reinterpret_cast<const VkBufferCreateInfo*>(&buffer_create_info),
                                 &alloc_create_info,
                                 &buffer,
                                 &allocation,
                                 &alloc_info);
    }

    if (result != VK_SUCCESS) {
        spdlog::error("vmaCreateBuffer failed: {}", vk::to_string(static_cast<vk::Result>(result)));
        throw std::runtime_error("Failed to create VkBuffer");
    }
    handle = buffer;
    mapped_data = alloc_info.pMappedData;
}

Buffer::~Buffer() {
    if (handle) {
        vmaDestroyBuffer(allocator, handle, allocation);
    }
}

Buffer::Buffer(Buffer&& other) noexcept
    : handle(std::exchange(other.handle, nullptr)),
      allocation(std::exchange(other.allocation, nullptr)),
      allocator(other.allocator),
      mapped_data(std::exchange(other.mapped_data, nullptr)) {
    other.handle = nullptr;
    other.allocation = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (handle) {
        vmaDestroyBuffer(allocator, handle, allocation);
    }

    handle = std::exchange(other.handle, nullptr);
    allocation = std::exchange(other.allocation, nullptr);
    allocator = other.allocator;
    mapped_data = std::exchange(other.mapped_data, nullptr);

    return *this;
}

const vk::Buffer& Buffer::get() const {
    return handle;
}

VmaAllocation Buffer::get_allocation() const {
    return allocation;
}

vk::DeviceAddress Buffer::get_device_address(const Device& device) const {
    const vk::BufferDeviceAddressInfo info{.buffer = handle};
    return device.get().getBufferAddress(info);
}

BufferBuilder::BufferBuilder() = default;

BufferBuilder& BufferBuilder::size(const vk::DeviceSize size) {
    size_ = size;
    return *this;
}

BufferBuilder& BufferBuilder::usage(const vk::BufferUsageFlags usage) {
    usage_ = usage;
    return *this;
}

BufferBuilder& BufferBuilder::memory_usage(const VmaMemoryUsage memory_usage) {
    memory_usage_ = memory_usage;
    return *this;
}

BufferBuilder& BufferBuilder::allocation_flags(const VmaAllocationCreateFlags allocation_flags) {
    allocation_flags_ = allocation_flags;
    return *this;
}

BufferBuilder& BufferBuilder::min_alignment(const uint32_t alignment) {
    min_alignment_ = alignment;
    return *this;
}

Buffer BufferBuilder::build(const Allocator& allocator) const {
    return Buffer(allocator, size_, usage_, memory_usage_, allocation_flags_, min_alignment_);
}