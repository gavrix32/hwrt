#pragma once

class Instance;

class Adapter {
    vk::raii::PhysicalDevice vk_physical_device;

public:
    explicit Adapter(const Instance& instance, const std::vector<const char*>& required_extensions);
    const vk::raii::PhysicalDevice& get() const;
};