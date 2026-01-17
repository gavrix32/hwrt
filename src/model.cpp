#include "model.h"

#include "context.h"
#include "glm/gtc/type_ptr.hpp"
#include "spdlog/spdlog.h"
#include "vulkan/buffer.h"
#include "vulkan/encoder.h"
#include "vulkan/utils.h"

void process_mesh(const tinygltf::Model& gltf_model,
                  const tinygltf::Mesh& gltf_mesh,
                  std::vector<Vertex>& vertices,
                  std::vector<uint32_t>& indices,
                  std::vector<Mesh>& meshes) {
    Mesh mesh{};

    mesh.first_index = indices.size();
    mesh.vertex_offset = vertices.size();

    for (const auto& primitive : gltf_mesh.primitives) {
        if (primitive.indices < 0) {
            spdlog::error("Non indexed geometry in primitive");
            continue;
        }
        const auto& idx_accessor = gltf_model.accessors[primitive.indices];
        const auto& idx_buffer_view = gltf_model.bufferViews[idx_accessor.bufferView];
        const auto& idx_buffer = gltf_model.buffers[idx_buffer_view.buffer];

        const unsigned char* idx_data_ptr = idx_buffer.data.data() + idx_buffer_view.byteOffset + idx_accessor.byteOffset;
        const int idx_stride = idx_accessor.ByteStride(idx_buffer_view);

        const auto index_count = idx_accessor.count;
        indices.reserve(index_count);

        for (auto i = 0; i < index_count; ++i) {
            const unsigned char* index_ptr = idx_data_ptr + i * idx_stride;
            uint32_t index = 0;

            switch (idx_accessor.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    index = static_cast<uint32_t>(*index_ptr);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    index = static_cast<uint32_t>(*reinterpret_cast<const uint16_t*>(index_ptr));
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    index = *reinterpret_cast<const uint32_t*>(index_ptr);
                    break;
                default:
                    spdlog::critical("glTF unknown index component type: {}", idx_accessor.componentType);
            }

            indices.emplace_back(index + vertices.size());
        }
        const auto& pos_accessor = gltf_model.accessors[primitive.attributes.at("POSITION")];
        const auto& pos_buffer_view = gltf_model.bufferViews[pos_accessor.bufferView];
        const auto& pos_buffer = gltf_model.buffers[pos_buffer_view.buffer];

        const unsigned char* pos_data_ptr = pos_buffer.data.data() + pos_buffer_view.byteOffset + pos_accessor.byteOffset;
        const int pos_stride = pos_accessor.ByteStride(pos_buffer_view);

        for (auto i = 0; i < pos_accessor.count; ++i) {
            const auto pos_ptr = reinterpret_cast<const float*>(pos_data_ptr + i * pos_stride);

            vertices.emplace_back(Vertex{
                .position = {pos_ptr[0], pos_ptr[1], pos_ptr[2]},
                .normal = {0.0f, 0.0f, 0.0f},
                .tangent = {0.0f, 0.0f, 0.0f, 0.0f},
                .texcoord = {0.0f, 0.0f},
            });
        }
    }
    mesh.index_count = indices.size() - mesh.first_index;
    meshes.emplace_back(mesh);
}

void Model::process_node(const tinygltf::Model& gltf_model, const int node_index, const glm::mat4& parent_transform) {
    const auto& node = gltf_model.nodes[node_index];

    glm::mat4 local_transform;

    if (node.matrix.size() == 16) {
        local_transform = glm::make_mat4(node.matrix.data());
    } else {
        auto translation = glm::vec3(0.0f);
        auto rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        auto scale = glm::vec3(1.0f);

        if (node.translation.size() == 3) { translation = glm::make_vec3(node.translation.data()); }
        if (node.rotation.size() == 4) { rotation = glm::make_quat(node.rotation.data()); }
        if (node.scale.size() == 3) { scale = glm::make_vec3(node.scale.data()); }

        local_transform = glm::translate(glm::mat4(1.0f), translation) *
                          glm::mat4(rotation) *
                          glm::scale(glm::mat4(1.0f), scale);
    }

    const glm::mat4 global_transform = parent_transform * local_transform;

    if (node.mesh >= 0) {
        mesh_instances.emplace_back(node.mesh, global_transform);
    }
    for (const int child_index : node.children) {
        process_node(gltf_model, child_index, global_transform);
    }
}

void Model::create_vertex_buffer(const Allocator& allocator, const uint32_t vertex_count) {
    vertex_buffer = BufferBuilder()
                    .size(sizeof(Vertex) * vertex_count)
                    .usage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                           vk::BufferUsageFlagBits::eShaderDeviceAddress |
                           vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR)
                    .allocation_flags(
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                    .build(allocator);
}

void Model::create_index_buffer(const Allocator& allocator, const uint32_t index_count) {
    index_buffer = BufferBuilder()
                   .size(sizeof(uint32_t) * index_count)
                   .usage(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                          vk::BufferUsageFlagBits::eShaderDeviceAddress |
                          vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR)
                   .allocation_flags(
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                   .build(allocator);
}

void Model::build_blases(const Context& ctx,
                         vk::DeviceAddress vertex_address,
                         vk::DeviceAddress index_address,
                         std::vector<Mesh>& meshes,
                         uint32_t max_vertex) {
    auto as_props = ctx.get_adapter().get().getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceAccelerationStructurePropertiesKHR
    >().get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();

    auto single_time_encoder = SingleTimeEncoder(ctx.get_device());

    std::vector<Buffer> scratch_buffers;

    for (const auto& mesh : meshes) {
        vk::AccelerationStructureGeometryTrianglesDataKHR geometry_triangles_data{
            .vertexFormat = vk::Format::eR32G32B32Sfloat,
            .vertexData = vertex_address,
            .vertexStride = sizeof(Vertex),
            .maxVertex = max_vertex,
            .indexType = vk::IndexType::eUint32,
            .indexData = index_address + mesh.first_index * sizeof(uint32_t),
        };

        vk::AccelerationStructureGeometryKHR geometry{
            .geometryType = vk::GeometryTypeKHR::eTriangles,
            .geometry = geometry_triangles_data,
            .flags = vk::GeometryFlagBitsKHR::eOpaque,
        };

        vk::AccelerationStructureBuildRangeInfoKHR range_info{
            .primitiveCount = mesh.index_count / 3,
            .primitiveOffset = 0,
            .firstVertex = 0,
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

        blases.emplace_back(ctx.get_device(),
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

        geometry_info.dstAccelerationStructure = blases.back().get_handle();
        geometry_info.scratchData = scratch_buffer.get_device_address(ctx.get_device());

        single_time_encoder.get_cmd().buildAccelerationStructuresKHR({geometry_info}, {&range_info});
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


Model::Model(const Context& ctx, const tinygltf::Model& gltf_model) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Mesh> meshes;

    for (const auto& gltf_mesh : gltf_model.meshes) {
        spdlog::info("Processing mesh: {}", gltf_mesh.name);
        process_mesh(gltf_model, gltf_mesh, vertices, indices, meshes);
    }
    spdlog::info("{} faces", indices.size() / 3);

    const int scene_idx = gltf_model.defaultScene != -1 ? gltf_model.defaultScene : 0;
    const auto& gltf_scene = gltf_model.scenes[scene_idx];

    for (const int node_index : gltf_scene.nodes) {
        process_node(gltf_model, node_index, glm::mat4(1.0f));
    }

    create_vertex_buffer(ctx.get_allocator(), vertices.size());
    create_index_buffer(ctx.get_allocator(), indices.size());

    memcpy(vertex_buffer.mapped_ptr(), vertices.data(), sizeof(Vertex) * vertices.size());
    memcpy(index_buffer.mapped_ptr(), indices.data(), sizeof(u_int32_t) * indices.size());

    const auto vertex_address = vertex_buffer.get_device_address(ctx.get_device());
    const auto index_address = index_buffer.get_device_address(ctx.get_device());

    build_blases(ctx, vertex_address, index_address, meshes, vertices.size() - 1);
}