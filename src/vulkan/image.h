#pragma once

class Image {
    vk::Image handle;
    VmaAllocation vma_allocation{};
    VmaAllocator vma_allocator;

public:
    explicit Image(vk::ImageCreateInfo create_info, const Allocator& allocator,
                   VmaAllocationCreateFlags allocation_create_flags = 0);

    ~Image();

    // Disable copy
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    // Enable move
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    [[nodiscard]] const vk::Image& get() const;
    [[nodiscard]] VmaAllocation get_allocation() const;
};