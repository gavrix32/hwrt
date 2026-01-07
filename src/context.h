#pragma once

#include "vulkan/instance.h"
#include "vulkan/adapter.h"
#include "vulkan/device.h"
#include "vulkan/allocator.h"

class Context {
    Instance instance;
    Adapter adapter;
    Device device;
    Allocator allocator;

public:
    explicit Context(bool validation);

    [[nodiscard]] const Instance& get_instance() const;
    [[nodiscard]] const Adapter& get_adapter() const;
    [[nodiscard]] const Device& get_device() const;
    [[nodiscard]] const Allocator& get_allocator() const;
};