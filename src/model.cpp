#include "model.h"

#include <ranges>

#include "context.h"
#include "glm/gtc/type_ptr.hpp"
#include "spdlog/spdlog.h"
#include "vulkan/buffer.h"
#include "vulkan/encoder.h"
#include "vulkan/utils.h"

struct AttributeData {
    const uint8_t* p_data = nullptr;
    int stride = 0;
};

AttributeData get_attribute_data(const tinygltf::Model& model, const tinygltf::Primitive& primitive, const std::string& name) {
    const auto it = primitive.attributes.find(name);

    if (it == primitive.attributes.end()) {
        return {nullptr, 0};
    }

    const auto& accessor = model.accessors[it->second];
    const auto& buffer_view = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[buffer_view.buffer];

    return AttributeData{
        .p_data = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset,
        .stride = accessor.ByteStride(buffer_view)
    };
}

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
        const auto position = get_attribute_data(gltf_model, primitive, "POSITION");
        const auto normal = get_attribute_data(gltf_model, primitive, "NORMAL");
        const auto tangent = get_attribute_data(gltf_model, primitive, "TANGENT");
        const auto texcoord = get_attribute_data(gltf_model, primitive, "TEXCOORD_0");

        const size_t vertex_count = gltf_model.accessors[primitive.attributes.at("POSITION")].count;
        for (auto i = 0; i < vertex_count; ++i) {
            Vertex v{};

            auto p_pos = reinterpret_cast<const float*>(position.p_data + i * position.stride);
            v.position = {p_pos[0], p_pos[1], p_pos[2]};

            if (normal.p_data) {
                auto p_norm = reinterpret_cast<const float*>(normal.p_data + i * normal.stride);
                v.normal = {p_norm[0], p_norm[1], p_norm[2]};
            }

            if (tangent.p_data) {
                auto p_tan = reinterpret_cast<const float*>(tangent.p_data + i * tangent.stride);
                v.tangent = {p_tan[0], p_tan[1], p_tan[2], p_tan[3]};
            }

            if (texcoord.p_data) {
                auto p_uv = reinterpret_cast<const float*>(texcoord.p_data + i * texcoord.stride);
                v.texcoord = {p_uv[0], p_uv[1]};
            }

            vertices.emplace_back(v);
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
    SCOPED_TIMER();

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
        process_mesh(gltf_model, gltf_mesh, vertices, indices, meshes);
    }

    const int scene_idx = gltf_model.defaultScene != -1 ? gltf_model.defaultScene : 0;
    const auto& gltf_scene = gltf_model.scenes[scene_idx];

    for (const int node_index : gltf_scene.nodes) {
        process_node(gltf_model, node_index, glm::mat4(1.0f));
    }

    create_vertex_buffer(ctx.get_allocator(), vertices.size());
    create_index_buffer(ctx.get_allocator(), indices.size());

    memcpy(vertex_buffer.mapped_ptr(), vertices.data(), sizeof(Vertex) * vertices.size());
    memcpy(index_buffer.mapped_ptr(), indices.data(), sizeof(u_int32_t) * indices.size());

    vertex_address = vertex_buffer.get_device_address(ctx.get_device());
    index_address = index_buffer.get_device_address(ctx.get_device());

    gpu_meshes.reserve(meshes.size());

    for (const auto& mesh : meshes) {
        gpu_meshes.push_back({
            .vertex_address = vertex_address,
            .index_address = index_address,
            .first_index = mesh.first_index,
            .material_index = 0
        });
    }

    build_blases(ctx, vertex_address, index_address, meshes, vertices.size() - 1);
}