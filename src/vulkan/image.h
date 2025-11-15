#pragma once

class Image {
    vk::Image vk_image;
    VmaAllocation vma_allocation;
    VmaAllocator vma_allocator;

public:
    explicit Image(vk::ImageCreateInfo image_create_info, const Allocator& allocator,
                   VmaAllocationCreateFlags allocation_create_flags = 0);
    ~Image();
    const vk::Image& get() const;
    VmaAllocation get_allocation() const;
};