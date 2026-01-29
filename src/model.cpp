#include "model.h"

#include "glm/gtc/type_ptr.hpp"
#include "spdlog/spdlog.h"
#include "vulkan/buffer.h"

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

void Model::process_mesh(const tinygltf::Model& gltf_model, const tinygltf::Mesh& gltf_mesh) {
    Mesh mesh{};

    mesh.index_offset = indices.size();
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
    mesh.index_count = indices.size() - mesh.index_offset;
    mesh.vertex_count = vertices.size() - mesh.vertex_offset;
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

Model::Model(const tinygltf::Model& gltf_model) {
    for (const auto& gltf_mesh : gltf_model.meshes) {
        process_mesh(gltf_model, gltf_mesh);
    }

    const int scene_idx = gltf_model.defaultScene != -1 ? gltf_model.defaultScene : 0;
    const auto& gltf_scene = gltf_model.scenes[scene_idx];

    for (const int node_index : gltf_scene.nodes) {
        process_node(gltf_model, node_index, glm::mat4(1.0f));
    }

    spdlog::info("Loaded model with {} triangles", indices.size() / 3);
}