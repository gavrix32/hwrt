#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "buffer.h"

class AccelerationStructure {
    vk::raii::AccelerationStructureKHR handle = nullptr;
    vk::DeviceAddress address{};
    Buffer buffer;

public:
    AccelerationStructure() = default;

    explicit AccelerationStructure(const Device& device,
                                   const Allocator& allocator,
                                   const vk::AccelerationStructureBuildSizesInfoKHR& build_sizes,
                                   vk::AccelerationStructureTypeKHR type);

    // Move only
    AccelerationStructure(const AccelerationStructure&) = delete;
    AccelerationStructure& operator=(const AccelerationStructure&) = delete;
    AccelerationStructure(AccelerationStructure&& other) noexcept;
    AccelerationStructure& operator=(AccelerationStructure&& other) noexcept;

    [[nodiscard]] const vk::raii::AccelerationStructureKHR& get_handle() const { return handle; }
    [[nodiscard]] vk::DeviceAddress get_device_address() const { return address; }
};