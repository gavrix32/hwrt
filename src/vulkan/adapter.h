#pragma once

#include <vulkan/vulkan_raii.hpp>

class Instance;

class Adapter {
    vk::raii::PhysicalDevice handle;

public:
    explicit Adapter(const Instance& instance, const std::vector<const char*>& required_extensions);
    [[nodiscard]] const vk::raii::PhysicalDevice& get() const { return handle; }
};