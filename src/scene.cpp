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

void Scene::add_instance(const std::shared_ptr<Model>& model, const glm::mat4& transform, const Context& ctx) {
    const ModelInstance instance{
        .model = model,
        .transform = transform,
        .first_blas = static_cast<uint32_t>(blases.size())
    };

    for (auto& mesh : model->meshes) {
        Blas blas{};
        blas.geometry_offset = static_cast<uint32_t>(geometries.size());
        blas.geometry_count = static_cast<uint32_t>(mesh.primitives.size());

        for (auto& primitive : mesh.primitives) {
            Geometry geometry{
                .vertex_offset = static_cast<uint32_t>(vertices.size()),
                .vertex_count = static_cast<uint32_t>(primitive.vertices.size()),
                .index_offset = static_cast<uint32_t>(indices.size()),
                .index_count = static_cast<uint32_t>(primitive.indices.size()),
                .material_index = static_cast<uint32_t>(materials.size()) + primitive.material_index,
            };
            geometries.push_back(geometry);
            vertices.insert(vertices.end(), primitive.vertices.begin(), primitive.vertices.end());
            indices.insert(indices.end(), primitive.indices.begin(), primitive.indices.end());
        }
        blases.push_back(std::move(blas));
    }
    for (const auto& material : model->materials) {
        Material adjusted = material;
        if (adjusted.albedo_index != UINT32_MAX) adjusted.albedo_index += images.size();
        if (adjusted.normal_index != UINT32_MAX) adjusted.normal_index += images.size();
        if (adjusted.metallic_roughness_index != UINT32_MAX) adjusted.metallic_roughness_index += images.size();
        if (adjusted.emissive_index != UINT32_MAX) adjusted.emissive_index += images.size();
        materials.push_back(adjusted);
    }
    for (const auto& texture : model->textures) {
        auto image = ImageBuilder()
                     .type(vk::ImageType::e2D)
                     .format(vk::Format::eR8G8B8A8Srgb) // TODO: Unorm для не albedo
                     .size(texture.width, texture.height)
                     .mip_levels(1)
                     .layers(1)
                     .samples(vk::SampleCountFlagBits::e1)
                     .usage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
                     .build(ctx.get_allocator());

        image.upload_data(texture.data, texture.width * texture.height * 4, ctx.get_device());
        images.push_back(std::move(image));
    }
    model_instances.push_back(instance);
}

void Scene::build_blases(const Context& ctx) {
    spdlog::info("Building blases...");

    vertex_buffer = BufferBuilder()
                    .size(sizeof(Vertex) * vertices.size())
                    .usage(vk::BufferUsageFlagBits::eStorageBuffer |
                           vk::BufferUsageFlagBits::eShaderDeviceAddress |
                           vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR)
                    .allocation_flags(
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                    .build(ctx.get_allocator());

    index_buffer = BufferBuilder()
                   .size(sizeof(uint32_t) * indices.size())
                   .usage(vk::BufferUsageFlagBits::eStorageBuffer |
                          vk::BufferUsageFlagBits::eShaderDeviceAddress |
                          vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR)
                   .allocation_flags(
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                   .build(ctx.get_allocator());

    material_buffer = BufferBuilder()
                      .size(sizeof(Material) * materials.size())
                      .usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
                      .allocation_flags(
                          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                      .build(ctx.get_allocator());

    geometry_buffer = BufferBuilder()
                      .size(sizeof(Geometry) * geometries.size())
                      .usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
                      .allocation_flags(
                          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                      .build(ctx.get_allocator());

    memcpy(vertex_buffer.mapped_ptr(), vertices.data(), vertices.size() * sizeof(Vertex));
    memcpy(index_buffer.mapped_ptr(), indices.data(), indices.size() * sizeof(u_int32_t));
    memcpy(material_buffer.mapped_ptr(), materials.data(), materials.size() * sizeof(Material));
    memcpy(geometry_buffer.mapped_ptr(), geometries.data(), geometries.size() * sizeof(Geometry));

    auto vertex_address = vertex_buffer.get_device_address(ctx.get_device());
    auto index_address = index_buffer.get_device_address(ctx.get_device());
    auto material_address = material_buffer.get_device_address(ctx.get_device());
    auto geometry_address = geometry_buffer.get_device_address(ctx.get_device());

    scene_addresses = SceneAddresses{
        vertex_address,
        index_address,
        material_address,
        geometry_address
    };

    auto as_props = ctx.get_adapter().get().getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceAccelerationStructurePropertiesKHR
    >().get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();

    auto single_time_encoder = SingleTimeEncoder(ctx.get_device());

    std::vector<Buffer> scratch_buffers;

    for (auto& blas : blases) {
        std::vector<vk::AccelerationStructureGeometryKHR> as_geometries(blas.geometry_count);
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> as_ranges(blas.geometry_count);
        std::vector<uint32_t> max_counts(blas.geometry_count);

        for (uint32_t i = 0; i < blas.geometry_count; ++i) {
            auto& geometry = geometries[blas.geometry_offset + i];

            vk::AccelerationStructureGeometryTrianglesDataKHR triangles_data{
                .vertexFormat = vk::Format::eR32G32B32Sfloat,
                .vertexData = vertex_address + geometry.vertex_offset * sizeof(Vertex),
                .vertexStride = sizeof(Vertex),
                .maxVertex = geometry.vertex_count - 1,
                .indexType = vk::IndexType::eUint32,
                .indexData = index_address + geometry.index_offset * sizeof(uint32_t),
            };

            as_geometries[i] = vk::AccelerationStructureGeometryKHR{
                .geometryType = vk::GeometryTypeKHR::eTriangles,
                .geometry = triangles_data,
                .flags = vk::GeometryFlagBitsKHR::eOpaque,
            };

            as_ranges[i].primitiveCount = geometry.index_count / 3;
            max_counts[i] = geometry.index_count / 3;
        }

        vk::AccelerationStructureBuildGeometryInfoKHR build_info{
            .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
            .flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
            .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
            .geometryCount = static_cast<uint32_t>(as_geometries.size()),
            .pGeometries = as_geometries.data(),
        };

        auto build_sizes = ctx.get_device().get().getAccelerationStructureBuildSizesKHR(
            vk::AccelerationStructureBuildTypeKHR::eDevice,
            build_info,
            max_counts);

        blas.as = AccelerationStructure(ctx.get_device(),
                                        ctx.get_allocator(),
                                        build_sizes,
                                        vk::AccelerationStructureTypeKHR::eBottomLevel);

        auto& scratch_buffer = scratch_buffers.emplace_back(BufferBuilder()
                                                            .size(build_sizes.buildScratchSize)
                                                            .usage(
                                                                vk::BufferUsageFlagBits::eStorageBuffer |
                                                                vk::BufferUsageFlagBits::eShaderDeviceAddress)
                                                            .min_alignment(
                                                                as_props.minAccelerationStructureScratchOffsetAlignment)
                                                            .build(ctx.get_allocator()));

        build_info.dstAccelerationStructure = blas.as.get_handle();
        build_info.scratchData = scratch_buffer.get_device_address(ctx.get_device());

        single_time_encoder.get_cmd().buildAccelerationStructuresKHR({build_info}, as_ranges.data());
    }

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

    std::vector<vk::AccelerationStructureInstanceKHR> tlas_instances;

    for (const auto& model_instance : model_instances) {
        for (const auto& node : model_instance.model->nodes) {
            glm::mat4 final_transform = model_instance.transform * node.transform;
            uint32_t blas_idx = model_instance.first_blas + node.mesh_index;

            vk::AccelerationStructureInstanceKHR tlas_instance{
                .transform = vk_matrix(final_transform),
                .instanceCustomIndex = blases[blas_idx].geometry_offset,
                .mask = 0xFF,
                .instanceShaderBindingTableRecordOffset = 0,
                .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
                .accelerationStructureReference = blases[blas_idx].as.get_device_address()
            };
            tlas_instances.push_back(tlas_instance);
        }
    }

    if (tlas_instances.empty()) return;

    auto as_props = ctx.get_adapter().get().getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceAccelerationStructurePropertiesKHR
    >().get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();

    auto instance_buffer = BufferBuilder()
                           .size(tlas_instances.size() * sizeof(vk::AccelerationStructureInstanceKHR))
                           .usage(
                               vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                               vk::BufferUsageFlagBits::eShaderDeviceAddress)
                           .allocation_flags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                             VMA_ALLOCATION_CREATE_MAPPED_BIT)
                           .build(ctx.get_allocator());

    memcpy(instance_buffer.mapped_ptr(),
           tlas_instances.data(),
           tlas_instances.size() * sizeof(vk::AccelerationStructureInstanceKHR));

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
        .primitiveCount = static_cast<uint32_t>(tlas_instances.size()),
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    };

    std::vector max_counts{range_info.primitiveCount};

    auto sizes_info = ctx.get_device().get().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        geometry_info,
        max_counts);

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

    geometry_info.dstAccelerationStructure = tlas.get_handle();
    geometry_info.scratchData = scratch_buffer.get_device_address(ctx.get_device());

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