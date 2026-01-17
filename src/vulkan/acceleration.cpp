#include "acceleration.h"

#include "device.h"

AccelerationStructure::AccelerationStructure(const Device& device,
                                             const Allocator& allocator,
                                             const vk::AccelerationStructureBuildSizesInfoKHR& sizes_info,
                                             const vk::AccelerationStructureTypeKHR type) {
    buffer = BufferBuilder()
             .size(sizes_info.accelerationStructureSize)
             .usage(
                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                 vk::BufferUsageFlagBits::eShaderDeviceAddress)
             .build(allocator);

    const vk::AccelerationStructureCreateInfoKHR create_info{
        .buffer = buffer.get(),
        .size = sizes_info.accelerationStructureSize,
        .type = type,
    };
    handle = device.get().createAccelerationStructureKHR(create_info);

    const vk::AccelerationStructureDeviceAddressInfoKHR device_address_info{
        .accelerationStructure = handle
    };
    address = device.get().getAccelerationStructureAddressKHR(device_address_info);
}