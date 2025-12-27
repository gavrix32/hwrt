#pragma once

class Buffer {
    vk::Buffer handle;
    VmaAllocation vma_allocation{};
    VmaAllocator vma_allocator;

public:
    explicit Buffer(const Allocator& allocator, vk::DeviceSize size, vk::BufferUsageFlags usage,
                    VmaAllocationCreateFlags allocation_create_flags = 0);

    ~Buffer();

    // Disable copy
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Enable move
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    [[nodiscard]] const vk::Buffer& get() const;
    [[nodiscard]] VmaAllocation get_allocation() const;
};