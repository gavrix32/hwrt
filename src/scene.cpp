#include "scene.h"

#include "context.h"
#include "vulkan/encoder.h"

// GLM 4x4 column-major to Vulkan 3x4 row-major matrix
vk::TransformMatrixKHR vk_matrix(const glm::mat4& m) {
    const glm::mat4 t = glm::transpose(m);
    vk::TransformMatrixKHR out;
    memcpy(&out, &t, sizeof(float) * 12);
    return out;
}

void Scene::build_tlas(const Context& ctx) {
    std::vector<vk::AccelerationStructureInstanceKHR> instances;

    // TODO: to func
    // const Buffer gpu_meshes_buffer = BufferBuilder()
    //                                  .size(sizeof(GpuMesh) * gpu_meshes.size())
    //                                  .usage(
    //                                      vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
    //                                  .allocation_flags(
    //                                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
    //                                      VMA_ALLOCATION_CREATE_MAPPED_BIT)
    //                                  .build(ctx.get_allocator());
    //
    // memcpy(gpu_meshes_buffer.mapped_ptr(), gpu_meshes.data(), sizeof(GpuMesh) * gpu_meshes.size());
    // gpu_meshes_address = gpu_meshes_buffer.get_device_address(ctx.get_device());

    for (const auto& model_instance : model_instances) {
        for (const auto& mesh_instance : model_instance.model.mesh_instances) {
            const auto& blas = model_instance.model.blases[mesh_instance.mesh_index];

            glm::mat4 final_transform = model_instance.transform * mesh_instance.transform;

            vk::AccelerationStructureInstanceKHR tlas_instance{
                .transform = vk_matrix(final_transform),
                .instanceCustomIndex = mesh_instance.mesh_index,
                .mask = 0xFF,
                .instanceShaderBindingTableRecordOffset = 0,
                .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
                .accelerationStructureReference = blas.get_device_address()
            };
            instances.push_back(tlas_instance);
        }
    }

    if (instances.empty()) return;

    auto as_props = ctx.get_adapter().get().getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceAccelerationStructurePropertiesKHR
    >().get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();

    auto instance_buffer = BufferBuilder()
                           .size(instances.size() * sizeof(vk::AccelerationStructureInstanceKHR))
                           .usage(
                               vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                               vk::BufferUsageFlagBits::eShaderDeviceAddress)
                           .allocation_flags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                             VMA_ALLOCATION_CREATE_MAPPED_BIT)
                           .build(ctx.get_allocator());

    memcpy(instance_buffer.mapped_ptr(),
           instances.data(),
           instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));

    auto instance_device_address = instance_buffer.get_device_address(ctx.get_device());

    vk::AccelerationStructureGeometryInstancesDataKHR instances_data{
        .arrayOfPointers = vk::False,
        .data = instance_device_address,
    };

    vk::AccelerationStructureGeometryKHR geometry{
        .geometryType = vk::GeometryTypeKHR::eInstances,
        .geometry = instances_data,
        .flags = vk::GeometryFlagBitsKHR::eOpaque,
    };

    vk::AccelerationStructureBuildGeometryInfoKHR geometry_info{
        .type = vk::AccelerationStructureTypeKHR::eTopLevel,
        .flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries = &geometry,
    };

    vk::AccelerationStructureBuildRangeInfoKHR range_info{
        .primitiveCount = static_cast<uint32_t>(instances.size()),
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    };

    std::vector max_primitives{range_info.primitiveCount};

    auto sizes_info = ctx.get_device().get().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        geometry_info,
        max_primitives);

    tlas = AccelerationStructure(
        ctx.get_device(),
        ctx.get_allocator(),
        sizes_info,
        vk::AccelerationStructureTypeKHR::eTopLevel);

    auto scratch_buffer = BufferBuilder()
                          .size(sizes_info.buildScratchSize)
                          .usage(
                              vk::BufferUsageFlagBits::eStorageBuffer |
                              vk::BufferUsageFlagBits::eShaderDeviceAddress)
                          .min_alignment(as_props.minAccelerationStructureScratchOffsetAlignment)
                          .build(ctx.get_allocator());

    auto scratch_buffer_device_address = scratch_buffer.get_device_address(ctx.get_device());

    geometry_info.dstAccelerationStructure = tlas.get_handle();
    geometry_info.scratchData = scratch_buffer_device_address;

    auto single_time_encoder = SingleTimeEncoder(ctx.get_device());

    single_time_encoder.get_cmd().buildAccelerationStructuresKHR({geometry_info}, {&range_info});

    vk::MemoryBarrier2 build_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        .srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
        .dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
        .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR,
    };

    vk::DependencyInfo dependency_info{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &build_barrier,
    };

    single_time_encoder.get_cmd().pipelineBarrier2(dependency_info);

    single_time_encoder.submit(ctx.get_device());
}