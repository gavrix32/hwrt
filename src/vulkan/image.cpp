#include "image.h"
#include "utils.h"

#include <spdlog/spdlog.h>

Image::Image(vk::ImageCreateInfo create_info, const Allocator& allocator,
             const VmaAllocationCreateFlags allocation_create_flags) : vma_allocator(allocator.get()) {
    SCOPED_TIMER();

    const VmaAllocationCreateInfo allocation_create_info = {
        .flags = allocation_create_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VkImage image;
    const auto result = vmaCreateImage(allocator.get(),
                                       create_info,
                                       &allocation_create_info,
                                       &image,
                                       &vma_allocation,
                                       nullptr);

    if (result != VK_SUCCESS) {
        spdlog::error("vmaCreateImage failed: {}", vk::to_string(static_cast<vk::Result>(result)));
    }
    handle = image;
}

Image::Image(const vk::Image handle) : handle(handle), vma_allocator(nullptr) {
}

Image::~Image() {
    if (handle && vma_allocation && vma_allocator) {
        vmaDestroyImage(vma_allocator, handle, vma_allocation);
    }
}

Image::Image(Image&& other) noexcept
    : handle(other.handle),
      layout(other.layout),
      stage_mask(other.stage_mask),
      access_mask(other.access_mask),
      vma_allocation(other.vma_allocation),
      vma_allocator(other.vma_allocator) {
    other.handle = nullptr;
    other.vma_allocation = nullptr;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (handle && vma_allocation && vma_allocator) {
        vmaDestroyImage(vma_allocator, handle, vma_allocation);
    }

    handle = std::exchange(other.handle, nullptr);
    layout = other.layout;
    stage_mask = other.stage_mask;
    access_mask = other.access_mask;
    vma_allocation = std::exchange(other.vma_allocation, nullptr);
    vma_allocator = other.vma_allocator;

    return *this;
}

void Image::transition_layout(const vk::raii::CommandBuffer& cmd, const vk::ImageLayout new_layout,
                              const vk::PipelineStageFlags2 new_stage_mask, const vk::AccessFlags2 new_access_mask) {
    // SCOPED_TIMER_NAMED("Transition layout image ({} -> {})", vk::to_string(layout), vk::to_string(new_layout));

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
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
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