#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#include "instance.h"

class Adapter {
    vk::raii::PhysicalDevice handle;

public:
    explicit Adapter(const Instance& instance, const std::vector<const char*>& required_extensions);
    [[nodiscard]] const vk::raii::PhysicalDevice& get() const;
};