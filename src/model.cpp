#include "model.h"

#include "fastgltf/tools.hpp"
#include "fastgltf/glm_element_traits.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "spdlog/spdlog.h"
#include "vulkan/buffer.h"

glm::mat4 get_transform_matrix(const fastgltf::Node& node) {
    const auto& transform = node.transform;

    if (const auto* trs = std::get_if<fastgltf::TRS>(&transform)) {
        const auto t = glm::make_vec3(trs->translation.data());
        const auto r = glm::make_quat(trs->rotation.data());
        const auto s = glm::make_vec3(trs->scale.data());

        return glm::translate(glm::mat4(1.0f), t) *
               glm::mat4(r) *
               glm::scale(glm::mat4(1.0f), s);
    }
    if (const auto* mat = std::get_if<fastgltf::math::fmat4x4>(&transform)) {
        return glm::make_mat4(mat->data());
    }

    return glm::mat4(1.0f);
}

void Model::process_mesh(const fastgltf::Asset& asset, const fastgltf::Mesh& gltf_mesh) {
    Mesh mesh{};

    mesh.index_offset = indices.size();
    mesh.vertex_offset = vertices.size();

    for (const auto& primitive : gltf_mesh.primitives) {
        auto* pos_iter = primitive.findAttribute("POSITION");
        if (pos_iter == primitive.attributes.end()) continue;

        const auto& pos_accessor = asset.accessors[pos_iter->accessorIndex];

        const size_t vertex_count = pos_accessor.count;
        size_t base_vertex = vertices.size();
        vertices.resize(base_vertex + vertex_count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, pos_accessor, [&](const glm::vec3 pos, const size_t idx) {
            vertices[base_vertex + idx].position = pos;
        });

        if (const auto* norm_iter = primitive.findAttribute("NORMAL"); norm_iter != primitive.attributes.end()) {
            const auto& norm_accessor = asset.accessors[norm_iter->accessorIndex];
            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, norm_accessor, [&](const glm::vec3 norm, const size_t idx) {
                vertices[base_vertex + idx].normal = norm;
            });
        }

        if (const auto* tan_iter = primitive.findAttribute("TANGENT"); tan_iter != primitive.attributes.end()) {
            const auto& tan_accessor = asset.accessors[tan_iter->accessorIndex];
            fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, tan_accessor, [&](const glm::vec4 tan, const size_t idx) {
                vertices[base_vertex + idx].tangent = tan;
            });
        }

        if (const auto* uv_iter = primitive.findAttribute("TEXCOORD_0"); uv_iter != primitive.attributes.end()) {
            const auto& uv_accessor = asset.accessors[uv_iter->accessorIndex];
            fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, uv_accessor, [&](const glm::vec2 uv, const size_t idx) {
                vertices[base_vertex + idx].texcoord = uv;
            });
        }

        if (primitive.indicesAccessor.has_value()) {
            const auto& accessor = asset.accessors[primitive.indicesAccessor.value()];
            indices.reserve(indices.size() + accessor.count);
            fastgltf::iterateAccessor<std::uint32_t>(asset, accessor, [&](const std::uint32_t index) {
                indices.push_back(base_vertex + index);
            });
        }
    }
    mesh.index_count = indices.size() - mesh.index_offset;
    mesh.vertex_count = vertices.size() - mesh.vertex_offset;
    meshes.emplace_back(mesh);
}

void Model::process_node(const fastgltf::Asset& asset, const size_t node_index, const glm::mat4& parent_transform) {
    const auto& node = asset.nodes[node_index];

    const glm::mat4 local_transform = get_transform_matrix(node);
    const glm::mat4 global_transform = parent_transform * local_transform;

    if (node.meshIndex.has_value()) {
        mesh_instances.emplace_back(node.meshIndex.value(), global_transform);
    }

    for (const size_t child_index : node.children) {
        process_node(asset, child_index, global_transform);
    }
}

Model::Model(const fastgltf::Asset& asset) {
    for (const auto& gltf_mesh : asset.meshes) {
        process_mesh(asset, gltf_mesh);
    }

    size_t scene_index = 0;
    if (asset.defaultScene.has_value()) {
        scene_index = asset.defaultScene.value();
    }
    const auto& gltf_scene = asset.scenes[scene_index];

    for (const size_t node_index : gltf_scene.nodeIndices) {
        process_node(asset, node_index, glm::mat4(1.0f));
    }

    spdlog::info("Loaded model with {} triangles", indices.size() / 3);
}