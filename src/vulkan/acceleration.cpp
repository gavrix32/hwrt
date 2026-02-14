#include "acceleration.h"

#include "device.h"

AccelerationStructure::AccelerationStructure(const Device& device,
                                             const Allocator& allocator,
                                             const vk::AccelerationStructureBuildSizesInfoKHR& build_sizes,
                                             const vk::AccelerationStructureTypeKHR type) {
    buffer = BufferBuilder()
             .size(build_sizes.accelerationStructureSize)
             .usage(
                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                 vk::BufferUsageFlagBits::eShaderDeviceAddress)
             .build(allocator);

    const vk::AccelerationStructureCreateInfoKHR create_info{
        .buffer = buffer.get(),
        .size = build_sizes.accelerationStructureSize,
        .type = type,
    };
    handle = device.get().createAccelerationStructureKHR(create_info);

    const vk::AccelerationStructureDeviceAddressInfoKHR device_address_info{
        .accelerationStructure = handle
    };
    address = device.get().getAccelerationStructureAddressKHR(device_address_info);
}