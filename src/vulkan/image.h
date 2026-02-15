#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <vk_mem_alloc.h>

#include "allocator.h"

class Image {
    vk::Image handle;

    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    vk::PipelineStageFlags2 stage_mask = vk::PipelineStageFlagBits2::eTopOfPipe;
    vk::AccessFlags2 access_mask = vk::AccessFlagBits2::eNone;

    uint32_t mip_levels_ = 1;
    vk::Extent3D extent_{};

    VmaAllocation vma_allocation{};
    VmaAllocator allocator;

public:
    uint32_t layers_ = 1;
    vk::Format format_ = vk::Format::eUndefined;

    explicit Image(vk::ImageType type,
                   vk::Format format,
                   uint32_t width,
                   uint32_t height,
                   uint32_t mip_levels,
                   uint32_t layers,
                   vk::SampleCountFlagBits samples,
                   vk::ImageUsageFlags usage,
                   const Allocator& allocator,
                   VmaMemoryUsage memory_usage,
                   VmaAllocationCreateFlags allocation_flags);

    explicit Image(vk::Image handle);

    void upload_data(const void* data, vk::DeviceSize size, const Device& device);

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

class ImageBuilder {
    vk::ImageType type_ = vk::ImageType::e2D;
    vk::ImageCreateFlags flags_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t layers_ = 1;
    uint32_t mip_levels_ = 1;
    bool generate_mip_levels_ = false;
    vk::Format format_;
    vk::ImageUsageFlags usage_;
    vk::SampleCountFlagBits samples_ = vk::SampleCountFlagBits::e1;

    VmaMemoryUsage memory_usage_ = VMA_MEMORY_USAGE_AUTO;
    VmaAllocationCreateFlags allocation_flags_ = 0;

public:
    explicit ImageBuilder();

    ImageBuilder& type(vk::ImageType type);
    ImageBuilder& flags(vk::ImageCreateFlags flags);
    ImageBuilder& size(uint32_t width, uint32_t height);
    ImageBuilder& layers(uint32_t layers);
    ImageBuilder& mip_levels(uint32_t levels);
    ImageBuilder& generate_mip_levels(bool generate);
    ImageBuilder& format(vk::Format format);
    ImageBuilder& usage(vk::ImageUsageFlags usage);
    ImageBuilder& samples(vk::SampleCountFlagBits samples);
    ImageBuilder& memory_usage(VmaMemoryUsage memory_usage);
    ImageBuilder& allocation_flags(VmaAllocationCreateFlags allocation_flags);

    [[nodiscard]] Image build(const Allocator& allocator) const;
};

class ImageView {
    vk::raii::ImageView handle;

public:
    explicit ImageView(const Device& device,
                       const Image& image,
                       vk::ImageViewType type,
                       vk::ImageAspectFlags aspect,
                       uint32_t base_mip_level,
                       uint32_t level_count);

    [[nodiscard]] const vk::raii::ImageView& get() const { return handle; }
};