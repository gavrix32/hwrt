#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#include "encoder.h"
#include "device.h"
#include "utils.h"

Encoder::Encoder(const Device& device, uint32_t max_command_buffers) : command_pool(nullptr) {
    SCOPED_TIMER_NAMED(fmt::format("Create VkCommandPool, VkCommandBuffer ({})", max_command_buffers));

    const vk::CommandPoolCreateInfo command_pool_create_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = device.get_queue_family_index(),
    };
    command_pool = device.get().createCommandPool(command_pool_create_info);

    const vk::CommandBufferAllocateInfo command_buffer_allocate_info{
        .commandPool = command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = max_command_buffers,
    };
    command_buffers = device.get().allocateCommandBuffers(command_buffer_allocate_info);
}

void Encoder::begin(const uint32_t index) {
    this->command_buffer_index = index;
    command_buffers[index].begin(vk::CommandBufferBeginInfo{});
}

void Encoder::end() const {
    command_buffers[command_buffer_index].end();
}

const vk::raii::CommandBuffer& Encoder::get_cmd() const {
    return command_buffers[command_buffer_index];
}

SingleTimeEncoder::SingleTimeEncoder(const Device& device) : command_pool(nullptr), command_buffer(nullptr) {
    SCOPED_TIMER_NAMED("Create VkCommandPool, VkCommandBuffer");

    const vk::CommandPoolCreateInfo command_pool_create_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = device.get_queue_family_index(),
    };
    command_pool = device.get().createCommandPool(command_pool_create_info);

    const vk::CommandBufferAllocateInfo command_buffer_allocate_info{
        .commandPool = command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };
    command_buffer = std::move(device.get().allocateCommandBuffers(command_buffer_allocate_info).front());

    constexpr vk::CommandBufferBeginInfo command_buffer_begin_info{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };
    command_buffer.begin(command_buffer_begin_info);
}

void SingleTimeEncoder::submit(const vk::raii::Queue& queue) const {
    command_buffer.end();

    const vk::SubmitInfo submit_info{
        .commandBufferCount = 1,
        .pCommandBuffers = &*command_buffer,
    };
    queue.submit(submit_info);
}

const vk::raii::CommandBuffer& SingleTimeEncoder::get_cmd() const {
    return command_buffer;
}
