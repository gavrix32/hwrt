#pragma once

#include "buffer.h"

class Adapter;
class RayTracingPipeline;

class ShaderBindingTable {
    Buffer buffer;
    vk::StridedDeviceAddressRegionKHR rgen_region{};
    vk::StridedDeviceAddressRegionKHR rmiss_region{};
    vk::StridedDeviceAddressRegionKHR hit_region{};

public:
    explicit ShaderBindingTable(const Adapter& adapter, const Device& device, const RayTracingPipeline& pipeline, const Allocator& allocator);

    // Move only
    ShaderBindingTable(const ShaderBindingTable&) = delete;
    ShaderBindingTable& operator=(const ShaderBindingTable&) = delete;
    ShaderBindingTable(ShaderBindingTable&& other) noexcept;
    ShaderBindingTable& operator=(ShaderBindingTable&& other) noexcept;

    [[nodiscard]] const Buffer& get_buffer() const { return buffer; }
    [[nodiscard]] const vk::StridedDeviceAddressRegionKHR& get_rgen_region() const { return rgen_region; }
    [[nodiscard]] const vk::StridedDeviceAddressRegionKHR& get_rmiss_region() const { return rmiss_region; }
    [[nodiscard]] const vk::StridedDeviceAddressRegionKHR& get_hit_region() const { return hit_region; }
};