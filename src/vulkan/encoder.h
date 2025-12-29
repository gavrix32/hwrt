#pragma once

#include <vulkan/vulkan_raii.hpp>

class Device;

class Encoder {
    vk::raii::CommandPool command_pool;
    std::vector<vk::raii::CommandBuffer> command_buffers;
    uint32_t command_buffer_index = 0;

public:
    explicit Encoder(const Device& device, uint32_t max_command_buffers);
    void begin(uint32_t index);
    void end() const;
    const vk::raii::CommandBuffer& get_cmd() const;
};

class SingleTimeEncoder {
    vk::raii::CommandPool command_pool;
    vk::raii::CommandBuffer command_buffer;

public:
    explicit SingleTimeEncoder(const Device& device);
    void submit(const vk::raii::Queue& queue) const;
    const vk::raii::CommandBuffer& get_cmd() const;
};