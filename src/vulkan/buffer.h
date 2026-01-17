#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <vk_mem_alloc.h>

class Allocator;
class Device;

class Buffer {
    vk::Buffer handle;
    VmaAllocator allocator{};
    VmaAllocation allocation{};
    void* mapped_data = nullptr;

public:
    Buffer() = default;

    explicit Buffer(const Allocator& allocator,
                    vk::DeviceSize size,
                    vk::BufferUsageFlags usage,
                    VmaMemoryUsage memory_usage,
                    VmaAllocationCreateFlags allocation_flags,
                    uint32_t min_alignment);

    ~Buffer();

    // Move only
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    [[nodiscard]] const vk::Buffer& get() const;
    [[nodiscard]] VmaAllocation get_allocation() const;
    [[nodiscard]] vk::DeviceAddress get_device_address(const Device& device) const;

    template <typename T = void>
    T* mapped_ptr() const { return static_cast<T*>(mapped_data); }
};

class BufferBuilder {
    vk::DeviceSize size_ = 0;
    vk::BufferUsageFlags usage_;
    VmaMemoryUsage memory_usage_ = VMA_MEMORY_USAGE_AUTO;
    VmaAllocationCreateFlags allocation_flags_ = 0;
    uint32_t min_alignment_ = 0;

public:
    explicit BufferBuilder();

    BufferBuilder& size(vk::DeviceSize size);
    BufferBuilder& usage(vk::BufferUsageFlags usage);
    BufferBuilder& memory_usage(VmaMemoryUsage memory_usage);
    BufferBuilder& allocation_flags(VmaAllocationCreateFlags allocation_flags);
    BufferBuilder& min_alignment(uint32_t alignment);

    [[nodiscard]] Buffer build(const Allocator& allocator) const;
};