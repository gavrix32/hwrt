#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <vk_mem_alloc.h>

#include "allocator.h"

class Image {
    vk::Image handle;

    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    vk::PipelineStageFlags2 stage_mask = vk::PipelineStageFlagBits2::eTopOfPipe;
    vk::AccessFlags2 access_mask = vk::AccessFlagBits2::eNone;

    VmaAllocation vma_allocation{};
    VmaAllocator vma_allocator;

public:
    explicit Image(vk::ImageCreateInfo create_info, const Allocator& allocator,
                   VmaAllocationCreateFlags allocation_create_flags = 0);

    explicit Image(vk::Image handle);

    ~Image();

    // Move only
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    void transition_layout(const vk::raii::CommandBuffer& cmd,
                           vk::ImageLayout new_layout,
                           vk::PipelineStageFlags2 new_stage_mask,
                           vk::AccessFlags2 new_access_mask);

    [[nodiscard]] const vk::Image& get() const;
    [[nodiscard]] VmaAllocation get_allocation() const;
    [[nodiscard]] vk::ImageLayout get_layout() const;
};