#include "scene.h"

#include <spdlog/spdlog.h>

#include "context.h"
#include "vulkan/encoder.h"

// GLM 4x4 column-major to Vulkan 3x4 row-major matrix
vk::TransformMatrixKHR vk_matrix(const glm::mat4& m) {
    const glm::mat4 t = glm::transpose(m);
    vk::TransformMatrixKHR out;
    memcpy(&out, &t, sizeof(float) * 12);
    return out;
}

void Scene::add_instance(const std::shared_ptr<Model>& model, const glm::mat4& transform, const Context& ctx) {
    if (model_cache.contains(model.get())) {
        model_instances.push_back({
            .model = model,
            .transform = transform,
            .first_blas = model_cache[model.get()]
        });
        return;
    }

    auto first_blas_idx = static_cast<uint32_t>(blases.size());

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

    std::vector is_srgb_texture(model->textures.size(), false);

    for (const auto& material : model->materials) {
        if (material.albedo_index != UINT32_MAX) is_srgb_texture[material.albedo_index] = true;
        if (material.emissive_index != UINT32_MAX) is_srgb_texture[material.emissive_index] = true;

        Material adjusted = material;
        if (adjusted.albedo_index != UINT32_MAX) adjusted.albedo_index += images.size();
        if (adjusted.normal_index != UINT32_MAX) adjusted.normal_index += images.size();
        if (adjusted.metallic_roughness_index != UINT32_MAX) adjusted.metallic_roughness_index += images.size();
        if (adjusted.emissive_index != UINT32_MAX) adjusted.emissive_index += images.size();
        materials.push_back(adjusted);
    }

    for (int i = 0; i < model->textures.size(); ++i) {
        const auto& texture = model->textures[i];

        auto image = ImageBuilder()
                     .type(vk::ImageType::e2D)
                     .format(is_srgb_texture[i] ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm)
                     .size(texture.width, texture.height)
                     .mip_levels(1)
                     .layers(1)
                     .samples(vk::SampleCountFlagBits::e1)
                     .usage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
                     .build(ctx.get_allocator());

        image.upload_data(texture.data, texture.width * texture.height * 4, ctx.get_device());
        image.metadata_flags = texture.metadata_flags;

        image_views.emplace_back(ctx.get_device(), image, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, 0, 1);
        images.push_back(std::move(image));
    }

    model_cache[model.get()] = first_blas_idx;

    model_instances.push_back({
        .model = model,
        .transform = transform,
        .first_blas = first_blas_idx
    });
}

void Scene::build_blases(const Context& ctx) {
    spdlog::info("Building blases...");

    if (blases.empty()) return;

    if (materials.empty()) {
        materials.push_back(Material{});
    }

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
    memcpy(index_buffer.mapped_ptr(), indices.data(), indices.size() * sizeof(uint32_t));
    memcpy(material_buffer.mapped_ptr(), materials.data(), materials.size() * sizeof(Material));
    memcpy(geometry_buffer.mapped_ptr(), geometries.data(), geometries.size() * sizeof(Geometry));

    auto vertex_address = vertex_buffer.get_device_address(ctx.get_device());
    auto index_address = index_buffer.get_device_address(ctx.get_device());
    auto material_address = material_buffer.get_device_address(ctx.get_device());
    auto geometry_address = geometry_buffer.get_device_address(ctx.get_device());

    scene_ptrs.vertices = vertex_address;
    scene_ptrs.indices = index_address;
    scene_ptrs.materials = material_address;
    scene_ptrs.geometries = geometry_address;

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

            auto geometry_flags = vk::GeometryFlagBitsKHR::eOpaque;

            if (materials[geometry.material_index].alpha_mode != AlphaMode::Opaque) {
                geometry_flags = {};
            }

            as_geometries[i] = vk::AccelerationStructureGeometryKHR{
                .geometryType = vk::GeometryTypeKHR::eTriangles,
                .geometry = triangles_data,
                .flags = geometry_flags,
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
            const uint32_t blas_idx = model_instance.first_blas + node.mesh_index;
            const auto& blas = blases[blas_idx];

            uint32_t sbt_offset = 0;

            for (uint32_t i = 0; i < blas.geometry_count; ++i) {
                uint32_t geometry_idx = blas.geometry_offset + i;
                const auto& material = materials[geometries[geometry_idx].material_index];

                if (material.alpha_mode != AlphaMode::Opaque) {
                    sbt_offset = 1;
                    break;
                }
            }

            vk::AccelerationStructureInstanceKHR tlas_instance{
                .transform = vk_matrix(final_transform),
                .instanceCustomIndex = blas.geometry_offset,
                .mask = 0xFF,
                .instanceShaderBindingTableRecordOffset = sbt_offset,
                .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
                .accelerationStructureReference = blas.as.get_device_address()
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

void Scene::build_light_buffer(const Context& ctx) {
    spdlog::info("Building light buffer...");

    for (const auto& instance : model_instances) {
        const auto& model = instance.model;
        for (const auto& node : model->nodes) {
            glm::mat4 world_transform = instance.transform * node.transform;

            const auto& mesh = model->meshes[node.mesh_index];
            for (const auto& primitive : mesh.primitives) {
                const auto& material = model->materials[primitive.material_index];
                if (material.emissive_factor.r == 0.0f &&
                    material.emissive_factor.g == 0.0f &&
                    material.emissive_factor.b == 0.0f &&
                    material.emissive_index == UINT32_MAX) {
                    continue;
                }

                const uint32_t num_triangles = primitive.indices.size() / 3;
                for (uint32_t triangle_idx = 0; triangle_idx < num_triangles; ++triangle_idx) {
                    const uint32_t i0 = primitive.indices[triangle_idx * 3 + 0];
                    const uint32_t i1 = primitive.indices[triangle_idx * 3 + 1];
                    const uint32_t i2 = primitive.indices[triangle_idx * 3 + 2];

                    const glm::vec3 v0 = world_transform * glm::vec4(primitive.vertices[i0].position, 1.0f);
                    const glm::vec3 v1 = world_transform * glm::vec4(primitive.vertices[i1].position, 1.0f);
                    const glm::vec3 v2 = world_transform * glm::vec4(primitive.vertices[i2].position, 1.0f);

                    const float area = 0.5f * glm::length(glm::cross(v1 - v0, v2 - v0));
                    if (area < 1e-6f) continue;

                    lights.push_back({
                        .emission = material.emissive_factor,
                        .v0 = v0,
                        .v1 = v1,
                        .v2 = v2,
                        .area = area
                    });
                }
            }
        }
    }

    if (lights.empty()) {
        lights.push_back({});
    }

    light_buffer = BufferBuilder()
                   .size(sizeof(Light) * lights.size())
                   .usage(vk::BufferUsageFlagBits::eShaderDeviceAddress)
                   .allocation_flags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                     VMA_ALLOCATION_CREATE_MAPPED_BIT)
                   .build(ctx.get_allocator());

    memcpy(light_buffer.mapped_ptr(), lights.data(), lights.size() * sizeof(Light));

    scene_ptrs.lights = light_buffer.get_device_address(ctx.get_device());
}

void Scene::build_descriptor_set(const Context& ctx) {
    spdlog::info("Building descriptor set...");

    if (MAX_TEXTURES < images.size()) {
        spdlog::error("MAX_TEXTURES is {} while the scene have {} textures", MAX_TEXTURES, images.size());
    }

    vk::DescriptorPoolSize pool_size{
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = MAX_TEXTURES
    };

    const vk::DescriptorPoolCreateInfo pool_info{
        .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    descriptor_pool = ctx.get_device().get().createDescriptorPool(pool_info);

    const vk::DescriptorSetAllocateInfo alloc_info{
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &*ctx.get_bindless_layout(),
    };

    descriptor_set = std::move(ctx.get_device().get().allocateDescriptorSets(alloc_info)[0]);

    std::vector<vk::DescriptorImageInfo> image_infos;
    std::vector<vk::WriteDescriptorSet> write_sets;

    image_infos.reserve(images.size());
    write_sets.reserve(images.size());

    for (uint32_t i = 0; i < images.size(); ++i) {
        image_infos.emplace_back(vk::DescriptorImageInfo{
            .sampler = images[i].metadata_flags & Image::FlagPlaceholder
                           ? ctx.get_nearest_sampler().get()
                           : ctx.get_linear_sampler().get(),
            .imageView = image_views[i].get(),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        });

        write_sets.emplace_back(vk::WriteDescriptorSet{
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = i,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &image_infos[i]
        });
    }
    ctx.get_device().get().updateDescriptorSets(write_sets, {});
}