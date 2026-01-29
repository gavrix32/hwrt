#include "scene.h"

#include "context.h"
#include "spdlog/spdlog.h"
#include "vulkan/encoder.h"

// GLM 4x4 column-major to Vulkan 3x4 row-major matrix
vk::TransformMatrixKHR vk_matrix(const glm::mat4& m) {
    const glm::mat4 t = glm::transpose(m);
    vk::TransformMatrixKHR out;
    memcpy(&out, &t, sizeof(float) * 12);
    return out;
}

void Scene::add_instance(const Model& model, const glm::mat4& transform) {
    ModelInfo info{};

    if (!unique_models.contains(&model)) {
        info = ModelInfo{
            .id = static_cast<uint32_t>(unique_models.size()),
            .first_blas_id = blas_count,
            .global_vertex_offset = static_cast<uint32_t>(global_vertices.size()),
            .global_index_offset = static_cast<uint32_t>(global_indices.size()),
            .vertex_count = static_cast<uint32_t>(model.vertices.size()),
            .index_count = static_cast<uint32_t>(model.indices.size()),
        };
        unique_models[&model] = info;

        blas_count = model.meshes.size();

        global_vertices.insert(global_vertices.end(), model.vertices.begin(), model.vertices.end());
        global_indices.insert(global_indices.end(), model.indices.begin(), model.indices.end());
    } else {
        info = unique_models.at(&model);
    }

    model_instances.push_back({model, transform, info});
}

void Scene::build_blases(const Context& ctx) {
    spdlog::info("Building blases...");

    global_vertex_buffer = BufferBuilder()
                           .size(sizeof(Vertex) * global_vertices.size())
                           .usage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                                  vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                  vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR)
                           .allocation_flags(
                               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                           .build(ctx.get_allocator());

    global_index_buffer = BufferBuilder()
                          .size(sizeof(uint32_t) * global_indices.size())
                          .usage(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                                 vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                 vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR)
                          .allocation_flags(
                              VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                          .build(ctx.get_allocator());

    memcpy(global_vertex_buffer.mapped_ptr(), global_vertices.data(), sizeof(Vertex) * global_vertices.size());
    memcpy(global_index_buffer.mapped_ptr(), global_indices.data(), sizeof(u_int32_t) * global_indices.size());

    auto as_props = ctx.get_adapter().get().getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceAccelerationStructurePropertiesKHR
    >().get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();

    auto single_time_encoder = SingleTimeEncoder(ctx.get_device());

    std::vector<Buffer> scratch_buffers;

    blases.resize(blas_count);
    gpu_meshes.resize(blas_count);

    for (const auto& [model_ptr, model_info] : unique_models) {
        for (auto i = 0; i < model_ptr->meshes.size(); ++i) {
            const auto& mesh = model_ptr->meshes[i];
            uint32_t blas_id = model_info.first_blas_id + i;

            uint64_t vertex_offset = model_info.global_vertex_offset * sizeof(Vertex);
            uint64_t index_offset = (model_info.global_index_offset + mesh.index_offset) * sizeof(uint32_t);

            gpu_meshes[blas_id] = GpuMesh{
                .vertex_address = global_vertex_buffer.get_device_address(ctx.get_device()) + vertex_offset,
                .index_address = global_index_buffer.get_device_address(ctx.get_device()) + index_offset,
            };

            vk::AccelerationStructureGeometryTrianglesDataKHR geometry_triangles_data{
                .vertexFormat = vk::Format::eR32G32B32Sfloat,
                .vertexData = global_vertex_buffer.get_device_address(ctx.get_device()),
                .vertexStride = sizeof(Vertex),
                .maxVertex = mesh.vertex_count - 1,
                .indexType = vk::IndexType::eUint32,
                .indexData = global_index_buffer.get_device_address(ctx.get_device()),
            };

            vk::AccelerationStructureGeometryKHR geometry{
                .geometryType = vk::GeometryTypeKHR::eTriangles,
                .geometry = geometry_triangles_data,
                .flags = vk::GeometryFlagBitsKHR::eOpaque,
            };

            vk::AccelerationStructureBuildRangeInfoKHR range_info{
                .primitiveCount = mesh.index_count / 3,
                .primitiveOffset = static_cast<uint32_t>((model_info.global_index_offset + mesh.index_offset) * sizeof(uint32_t)),
                .firstVertex = model_info.global_vertex_offset,
                .transformOffset = 0,
            };

            vk::AccelerationStructureBuildGeometryInfoKHR geometry_info{
                .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
                .flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
                .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
                .geometryCount = 1,
                .pGeometries = &geometry,
            };

            std::vector max_primitives{range_info.primitiveCount};

            auto sizes_info = ctx.get_device().get().getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                geometry_info,
                max_primitives);

            blases[blas_id] = AccelerationStructure(ctx.get_device(),
                                                    ctx.get_allocator(),
                                                    sizes_info,
                                                    vk::AccelerationStructureTypeKHR::eBottomLevel);

            auto& scratch_buffer = scratch_buffers.emplace_back(BufferBuilder()
                                                                .size(sizes_info.buildScratchSize)
                                                                .usage(
                                                                    vk::BufferUsageFlagBits::eStorageBuffer |
                                                                    vk::BufferUsageFlagBits::eShaderDeviceAddress)
                                                                .min_alignment(
                                                                    as_props.minAccelerationStructureScratchOffsetAlignment)
                                                                .build(ctx.get_allocator()));

            geometry_info.dstAccelerationStructure = blases[blas_id].get_handle();
            geometry_info.scratchData = scratch_buffer.get_device_address(ctx.get_device());

            single_time_encoder.get_cmd().buildAccelerationStructuresKHR({geometry_info}, {&range_info});
        }
    }

    gpu_mesh_buffer = BufferBuilder()
                      .size(sizeof(GpuMesh) * gpu_meshes.size())
                      .usage(
                          vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
                      .allocation_flags(
                          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT)
                      .build(ctx.get_allocator());

    memcpy(gpu_mesh_buffer.mapped_ptr(), gpu_meshes.data(), gpu_meshes.size() * sizeof(GpuMesh));

    vk::MemoryBarrier2 build_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        .srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
        .dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR,
    };

    vk::DependencyInfo dependency_info{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &build_barrier,
    };

    single_time_encoder.get_cmd().pipelineBarrier2(dependency_info);

    single_time_encoder.submit(ctx.get_device());
}

void Scene::build_tlas(const Context& ctx) {
    spdlog::info("Building tlas...");

    std::vector<vk::AccelerationStructureInstanceKHR> instances;

    for (const auto& model_instance : model_instances) {
        for (const auto& mesh_instance : model_instance.model.mesh_instances) {
            glm::mat4 final_transform = model_instance.transform * mesh_instance.transform;

            vk::AccelerationStructureInstanceKHR tlas_instance{
                .transform = vk_matrix(final_transform),
                .instanceCustomIndex = model_instance.info.first_blas_id + mesh_instance.mesh_index,
                .mask = 0xFF,
                .instanceShaderBindingTableRecordOffset = 0,
                .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
                .accelerationStructureReference = blases[model_instance.info.first_blas_id +
                                                         mesh_instance.mesh_index].get_device_address()
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