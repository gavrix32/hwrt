#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#include <vk_mem_alloc.h>

#include <spdlog/spdlog.h>

#include "instance.h"
#include "adapter.h"
#include "device.h"
#include "allocator.h"
#include "utils.h"

Allocator::Allocator(const Instance& instance, const Adapter& adapter, const Device& device) {
    SCOPED_TIMER_NAMED("Create VmaAllocator");

    const VmaAllocatorCreateInfo allocator_create_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = static_cast<vk::PhysicalDevice>(adapter.get()),
        .device = static_cast<vk::Device>(device.get()),
        .instance = static_cast<vk::Instance>(instance.get()),
    };

    VmaAllocator allocator;
    if (vmaCreateAllocator(&allocator_create_info, &allocator) != VK_SUCCESS) {
        spdlog::error("Failed to create allocator");
    }
    vma_allocator = allocator;
}

Allocator::~Allocator() {
    vmaDestroyAllocator(vma_allocator);
}

const VmaAllocator& Allocator::get() const {
    return vma_allocator;
}