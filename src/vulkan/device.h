#pragma once

class Adapter;

class Device {
    vk::raii::Device vk_device;
    vk::raii::Queue vk_queue;
    uint32_t queue_family_index;

public:
    explicit Device(const Adapter& adapter, const std::vector<const char*>& required_extensions,
                    vk::PhysicalDeviceFeatures2 features);
    const vk::raii::Device& get() const;
    const vk::raii::Queue& get_queue() const;
    uint32_t get_queue_family_index() const;
};