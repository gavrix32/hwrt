#include "sbt.h"

#include "adapter.h"
#include "pipeline.h"

inline uint32_t align_up(const uint32_t size, const uint32_t alignment) {
    return size + alignment - 1 & ~(alignment - 1);
}

ShaderBindingTable::ShaderBindingTable(const Adapter& adapter, const Device& device, const Pipeline& pipeline,
                                       const Allocator& allocator) {
    const auto pipeline_props = adapter.get().getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR
    >().get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    const uint32_t handle_size = pipeline_props.shaderGroupHandleSize;
    const uint32_t handle_alignment = pipeline_props.shaderGroupHandleAlignment;
    const uint32_t base_alignment = pipeline_props.shaderGroupBaseAlignment;

    const uint32_t handle_stride = align_up(handle_size, handle_alignment);

    const uint32_t group_count = pipeline.get_group_count();
    const size_t data_size = group_count * handle_size;

    const std::vector<uint8_t> shader_handles = pipeline.get().getRayTracingShaderGroupHandlesKHR<uint8_t>(
        0, group_count, data_size);

    const uint32_t rgen_count = pipeline.get_rgen_count();
    const uint32_t rmiss_count = pipeline.get_rmiss_count();
    const uint32_t hit_count = pipeline.get_hit_count();

    const vk::DeviceSize rgen_size = align_up(rgen_count * handle_stride, base_alignment);
    const vk::DeviceSize rmiss_size = align_up(rmiss_count * handle_stride, base_alignment);
    const vk::DeviceSize hit_size = align_up(hit_count * handle_stride, base_alignment);

    const vk::DeviceSize sbt_size = rgen_size + rmiss_size + hit_size;

    buffer = BufferBuilder()
             .size(sbt_size)
             .usage(
                 vk::BufferUsageFlagBits::eShaderDeviceAddress |
                 vk::BufferUsageFlagBits::eShaderBindingTableKHR)
             .allocation_flags(VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
             .build(allocator);

    const vk::DeviceAddress address = buffer.get_device_address(device);

    auto* p_data = buffer.mapped_ptr<uint8_t>();

    auto copy_region = [&](const uint32_t start_group, const uint32_t count, uint8_t* dst) {
        for (uint32_t i = 0; i < count; ++i) {
            memcpy(i * handle_stride + dst, handle_size * (start_group + i) + shader_handles.data(), handle_size);
        }
    };

    vk::DeviceSize offset = 0;

    // rgen region
    copy_region(0, rgen_count, p_data + offset);
    rgen_region = vk::StridedDeviceAddressRegionKHR{
        .deviceAddress = address + offset,
        .stride = handle_stride,
        .size = rgen_size
    };
    offset += rgen_size;

    // rmiss region
    if (rmiss_count > 0) {
        copy_region(rgen_count, rmiss_count, p_data + offset);
        rmiss_region = vk::StridedDeviceAddressRegionKHR{
            .deviceAddress = address + offset,
            .stride = handle_stride,
            .size = rmiss_size
        };
    }
    offset += rmiss_size;

    // hit region
    if (hit_count > 0) {
        copy_region(rgen_count + rmiss_count, hit_count, p_data + offset);
        hit_region = vk::StridedDeviceAddressRegionKHR{
            .deviceAddress = address + offset,
            .stride = handle_stride,
            .size = hit_size
        };
    }
}

ShaderBindingTable::ShaderBindingTable(ShaderBindingTable&& other) noexcept
    : buffer(std::move(other.buffer)),
      rgen_region(std::exchange(other.rgen_region, {})),
      rmiss_region(std::exchange(other.rmiss_region, {})),
      hit_region(std::exchange(other.hit_region, {})) {
}

ShaderBindingTable& ShaderBindingTable::operator=(ShaderBindingTable&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    buffer = std::move(other.buffer);
    rgen_region = std::exchange(other.rgen_region, {});
    rmiss_region = std::exchange(other.rmiss_region, {});
    hit_region = std::exchange(other.hit_region, {});

    return *this;
}