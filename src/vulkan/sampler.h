#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "device.h"

class Sampler {
    vk::raii::Sampler handle;

public:
    explicit Sampler(const Device& device, vk::Filter mag_filter, vk::Filter min_filter);
    [[nodiscard]] const vk::raii::Sampler& get() const { return handle; }
};