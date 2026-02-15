#include "image.h"
#include "utils.h"

#include <spdlog/spdlog.h>

#include "buffer.h"
#include "device.h"
#include "encoder.h"

Image::Image(const vk::ImageType type,
             const vk::Format format,
             const uint32_t width,
             const uint32_t height,
             const uint32_t mip_levels,
             const uint32_t layers,
             const vk::SampleCountFlagBits samples,
             const vk::ImageUsageFlags usage,
             const Allocator& allocator,
             const VmaMemoryUsage memory_usage,
             const VmaAllocationCreateFlags allocation_flags) :
    mip_levels_(mip_levels),
    extent_(vk::Extent3D{width, height, 1}),
    allocator(allocator.get()),
    layers_(layers),
    format_(format) {
    const vk::ImageCreateInfo create_info = {
        .imageType = type,
        .format = format_,
        .extent = extent_,
        .mipLevels = mip_levels,
        .arrayLayers = layers,
        .samples = samples,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    const VmaAllocationCreateInfo alloc_create_info = {
        .flags = allocation_flags,
        .usage = memory_usage,
    };

    VkImage image;
    const auto result = vmaCreateImage(allocator.get(),
                                       reinterpret_cast<const VkImageCreateInfo*>(&create_info),
                                       &alloc_create_info,
                                       &image,
                                       &vma_allocation,
                                       nullptr);

    if (result != VK_SUCCESS) {
        spdlog::error("vmaCreateImage failed: {}", vk::to_string(static_cast<vk::Result>(result)));
    }
    handle = image;
}

Image::Image(const vk::Image handle) : handle(handle), allocator(nullptr) {
}

Image::~Image() {
    if (handle && vma_allocation && allocator) {
        vmaDestroyImage(allocator, handle, vma_allocation);
    }
}

Image::Image(Image&& other) noexcept
    : handle(other.handle),
      layout(other.layout),
      stage_mask(other.stage_mask),
      access_mask(other.access_mask),
      vma_allocation(other.vma_allocation),
      allocator(other.allocator) {
    other.handle = nullptr;
    other.vma_allocation = nullptr;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (handle && vma_allocation && allocator) {
        vmaDestroyImage(allocator, handle, vma_allocation);
    }

    handle = std::exchange(other.handle, nullptr);
    layout = other.layout;
    stage_mask = other.stage_mask;
    access_mask = other.access_mask;
    vma_allocation = std::exchange(other.vma_allocation, nullptr);
    allocator = other.allocator;

    return *this;
}

void Image::transition_layout(const vk::raii::CommandBuffer& cmd,
                              const vk::ImageLayout new_layout,
                              const vk::PipelineStageFlags2 new_stage_mask,
                              const vk::AccessFlags2 new_access_mask) {
    // SCOPED_TIMER_MSG("Transition layout image ({} -> {})", vk::to_string(layout), vk::to_string(new_layout));

    vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = stage_mask,
        .srcAccessMask = access_mask,
        .dstStageMask = new_stage_mask,
        .dstAccessMask = new_access_mask,
        .oldLayout = layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = handle,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = mip_levels_,
            .baseArrayLayer = 0,
            .layerCount = layers_,
        }
    };

    const vk::DependencyInfo dependency_info{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    cmd.pipelineBarrier2(dependency_info);

    layout = new_layout;
    stage_mask = new_stage_mask;
    access_mask = new_access_mask;
}

const vk::Image& Image::get() const {
    return handle;
}

VmaAllocation Image::get_allocation() const {
    return vma_allocation;
}

vk::ImageLayout Image::get_layout() const {
    return layout;
}

void Image::upload_data(const void* data, const vk::DeviceSize size, const Device& device) {
    const vk::BufferCreateInfo staging_buffer_info = {
        .size = size,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
    };

    constexpr VmaAllocationCreateInfo staging_alloc_create_info = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    };

    VkBuffer staging_buffer;
    VmaAllocationInfo staging_alloc_info;
    VmaAllocation staging_allocation;

    VkResult result = vmaCreateBuffer(allocator,
                                      reinterpret_cast<const VkBufferCreateInfo*>(&staging_buffer_info),
                                      &staging_alloc_create_info,
                                      &staging_buffer,
                                      &staging_allocation,
                                      &staging_alloc_info);

    if (result != VK_SUCCESS) {
        spdlog::error("staging buffer vmaCreateBuffer failed: {}", vk::to_string(static_cast<vk::Result>(result)));
        throw std::runtime_error("Failed to create staging VkBuffer");
    }

    memcpy(staging_alloc_info.pMappedData, data, size);

    const auto single_time_encoder = SingleTimeEncoder(device);
    auto& cmd = single_time_encoder.get_cmd();

    transition_layout(cmd,
                      vk::ImageLayout::eTransferDstOptimal,
                      vk::PipelineStageFlagBits2::eTransfer,
                      vk::AccessFlagBits2::eTransferWrite);

    const vk::BufferImageCopy2 region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = layers_,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = extent_
    };

    const vk::CopyBufferToImageInfo2 copy_info{
        .srcBuffer = staging_buffer,
        .dstImage = handle,
        .dstImageLayout = layout,
        .regionCount = 1,
        .pRegions = &region
    };

    cmd.copyBufferToImage2(copy_info);

    transition_layout(cmd,
                      vk::ImageLayout::eShaderReadOnlyOptimal,
                      vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                      vk::AccessFlagBits2::eShaderRead);

    single_time_encoder.submit(device);

    vmaDestroyBuffer(allocator, staging_buffer, staging_allocation);
}

ImageBuilder::ImageBuilder() = default;

ImageBuilder& ImageBuilder::type(const vk::ImageType type) {
    type_ = type;
    return *this;
}

ImageBuilder& ImageBuilder::flags(const vk::ImageCreateFlags flags) {
    flags_ = flags;
    return *this;
}

ImageBuilder& ImageBuilder::size(const uint32_t width, const uint32_t height) {
    width_ = width;
    height_ = height;
    return *this;
}

ImageBuilder& ImageBuilder::layers(const uint32_t layers) {
    layers_ = layers;
    return *this;
}

ImageBuilder& ImageBuilder::mip_levels(const uint32_t levels) {
    mip_levels_ = levels;
    return *this;
}

ImageBuilder& ImageBuilder::generate_mip_levels(const bool generate) {
    spdlog::error("image generate_mip_levels() is not implemented");
    generate_mip_levels_ = generate;
    return *this;
}

ImageBuilder& ImageBuilder::format(const vk::Format format) {
    format_ = format;
    return *this;
}

ImageBuilder& ImageBuilder::usage(const vk::ImageUsageFlags usage) {
    usage_ = usage;
    return *this;
}

ImageBuilder& ImageBuilder::samples(const vk::SampleCountFlagBits samples) {
    samples_ = samples;
    return *this;
}

ImageBuilder& ImageBuilder::memory_usage(const VmaMemoryUsage memory_usage) {
    memory_usage_ = memory_usage;
    return *this;
}

ImageBuilder& ImageBuilder::allocation_flags(const VmaAllocationCreateFlags allocation_flags) {
    allocation_flags_ = allocation_flags;
    return *this;
}

Image ImageBuilder::build(const Allocator& allocator) const {
    return Image(type_,
                 format_,
                 width_,
                 height_,
                 mip_levels_,
                 layers_,
                 samples_,
                 usage_,
                 allocator,
                 memory_usage_,
                 allocation_flags_);
}

ImageView::ImageView(const Device& device,
                     const Image& image,
                     const vk::ImageViewType type,
                     const vk::ImageAspectFlags aspect,
                     const uint32_t base_mip_level,
                     const uint32_t level_count) : handle(nullptr) {
    const vk::ImageViewCreateInfo create_info{
        .image = image.get(),
        .viewType = type,
        .format = image.format_,
        .subresourceRange = {
            .aspectMask = aspect,
            .baseMipLevel = base_mip_level,
            .levelCount = level_count,
            .baseArrayLayer = 0,
            .layerCount = image.layers_
        }
    };
    handle = device.get().createImageView(create_info);
}