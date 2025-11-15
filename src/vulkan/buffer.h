#pragma once

class Allocator;

class Buffer {
    vk::Buffer vk_buffer;
    VmaAllocation vma_allocation;
    VmaAllocator vma_allocator;

public:
    explicit Buffer(const Allocator& allocator, vk::DeviceSize size, vk::BufferUsageFlags usage,
                    VmaAllocationCreateFlags allocation_create_flags = 0);
    ~Buffer();
    const vk::Buffer& get() const;
    VmaAllocation get_allocation() const;
};