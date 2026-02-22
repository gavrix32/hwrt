#pragma once

#include "vulkan/instance.h"
#include "vulkan/adapter.h"
#include "vulkan/device.h"
#include "vulkan/allocator.h"
#include "vulkan/sampler.h"

#define MAX_TEXTURES 1024

class Context {
    Instance instance;
    Adapter adapter;
    Device device;
    Allocator allocator;
    Sampler linear_sampler;
    vk::raii::DescriptorSetLayout bindless_layout;

public:
    explicit Context(bool validation);

    [[nodiscard]] const Instance& get_instance() const {
        return instance;
    }

    [[nodiscard]] const Adapter& get_adapter() const {
        return adapter;
    }

    [[nodiscard]] const Device& get_device() const {
        return device;
    }

    [[nodiscard]] const Allocator& get_allocator() const {
        return allocator;
    }

    [[nodiscard]] const Sampler& get_linear_sampler() const {
        return linear_sampler;
    }

    [[nodiscard]] const vk::raii::DescriptorSetLayout& get_bindless_layout() const {
        return bindless_layout;
    }
};