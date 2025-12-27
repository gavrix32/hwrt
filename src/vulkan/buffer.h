#pragma once

class Allocator;

class Buffer {
    vk::Buffer handle;
    VmaAllocation vma_allocation{};
    VmaAllocator vma_allocator;

public:
    explicit Buffer(const Allocator& allocator, vk::DeviceSize size, vk::BufferUsageFlags usage,
                    VmaAllocationCreateFlags allocation_create_flags = 0);
    ~Buffer();
    [[nodiscard]] const vk::Buffer& get() const;
    [[nodiscard]] VmaAllocation get_allocation() const;
};