#pragma once

#include <vulkan/vulkan_raii.hpp>

class Adapter;

class Device {
    vk::raii::Device handle;
    vk::raii::Queue queue;
    uint32_t queue_family_index;

public:
    explicit Device(const Adapter& adapter, const std::vector<const char*>& required_extensions);
    const vk::raii::Device& get() const;
    const vk::raii::Queue& get_queue() const;
    uint32_t get_queue_family_index() const;
};
