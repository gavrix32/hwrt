#include "model.h"

#include "fastgltf/tools.hpp"
#include "fastgltf/glm_element_traits.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "spdlog/spdlog.h"
#include "vulkan/buffer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "texture.h"

void Model::process_mesh(const fastgltf::Asset& asset, const fastgltf::Mesh& gltf_mesh) {
    Mesh mesh{};

    mesh.primitives.reserve(gltf_mesh.primitives.size());
    for (const auto& gltf_primitive : gltf_mesh.primitives) {
        Primitive primitive{};

        auto* pos_iter = gltf_primitive.findAttribute("POSITION");
        if (pos_iter == gltf_primitive.attributes.end()) continue;

        const auto& pos_accessor = asset.accessors[pos_iter->accessorIndex];
        primitive.vertices.resize(pos_accessor.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, pos_accessor, [&](const glm::vec3 pos, const size_t idx) {
            primitive.vertices[idx].position = pos;
        });

        if (const auto* norm_iter = gltf_primitive.findAttribute("NORMAL"); norm_iter != gltf_primitive.attributes.end()) {
            const auto& norm_accessor = asset.accessors[norm_iter->accessorIndex];
            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, norm_accessor, [&](const glm::vec3 norm, const size_t idx) {
                primitive.vertices[idx].normal = norm;
            });
        }

        if (const auto* tan_iter = gltf_primitive.findAttribute("TANGENT"); tan_iter != gltf_primitive.attributes.end()) {
            const auto& tan_accessor = asset.accessors[tan_iter->accessorIndex];
            fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, tan_accessor, [&](const glm::vec4 tan, const size_t idx) {
                primitive.vertices[idx].tangent = tan;
            });
        }

        if (const auto* uv_iter = gltf_primitive.findAttribute("TEXCOORD_0"); uv_iter != gltf_primitive.attributes.end()) {
            const auto& uv_accessor = asset.accessors[uv_iter->accessorIndex];
            fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, uv_accessor, [&](const glm::vec2 uv, const size_t idx) {
                primitive.vertices[idx].texcoord = uv;
            });
        }

        if (gltf_primitive.indicesAccessor.has_value()) {
            const auto& accessor = asset.accessors[gltf_primitive.indicesAccessor.value()];
            primitive.indices.reserve(accessor.count);
            fastgltf::iterateAccessor<std::uint32_t>(asset, accessor, [&](const std::uint32_t index) {
                primitive.indices.push_back(index);
            });
        }

        if (gltf_primitive.materialIndex.has_value()) {
            primitive.material_index = gltf_primitive.materialIndex.value();
        }

        mesh.primitives.push_back(primitive);
    }

    meshes.emplace_back(mesh);
}

glm::mat4 get_transform_matrix(const fastgltf::Node& gltf_node) {
    const auto& transform = gltf_node.transform;

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

    return {1.0f};
}

void Model::process_node(const fastgltf::Asset& asset, const size_t node_index, const glm::mat4& parent_transform) {
    const auto& gltf_node = asset.nodes[node_index];

    const glm::mat4 local_transform = get_transform_matrix(gltf_node);
    const glm::mat4 global_transform = parent_transform * local_transform;

    if (gltf_node.meshIndex.has_value()) {
        nodes.emplace_back(gltf_node.meshIndex.value(), global_transform);
    }

    for (const size_t child_index : gltf_node.children) {
        process_node(asset, child_index, global_transform);
    }
}

TextureData create_placeholder_texture() {
    TextureData tex;
    tex.width = 2;
    tex.height = 2;
    tex.channels = 4;
    tex.data = static_cast<unsigned char*>(malloc(tex.width * tex.height * 4));

    if (tex.data) {
        tex.data[0] = 255;
        tex.data[1] = 0;
        tex.data[2] = 255;
        tex.data[3] = 255;
        tex.data[4] = 0;
        tex.data[5] = 0;
        tex.data[6] = 0;
        tex.data[7] = 255;
        tex.data[8] = 0;
        tex.data[9] = 0;
        tex.data[10] = 0;
        tex.data[11] = 255;
        tex.data[12] = 255;
        tex.data[13] = 0;
        tex.data[14] = 255;
        tex.data[15] = 255;
    }
    return tex;
}

void Model::process_texture(const fastgltf::Asset& asset, const fastgltf::Image& image) {
    TextureData texture;
    bool success = false;

    if (const auto* buffer_view_source = std::get_if<fastgltf::sources::BufferView>(&image.data)) {
        const auto& view = asset.bufferViews[buffer_view_source->bufferViewIndex];
        const auto& buffer = asset.buffers[view.bufferIndex];

        const uint8_t* raw_data = nullptr;

        if (const auto* view_src = std::get_if<fastgltf::sources::ByteView>(&buffer.data)) {
            raw_data = reinterpret_cast<const uint8_t*>(view_src->bytes.data());
        } else if (const auto* array_src = std::get_if<fastgltf::sources::Array>(&buffer.data)) {
            raw_data = reinterpret_cast<const uint8_t*>(array_src->bytes.data());
        } else if (const auto* vec_src = std::get_if<fastgltf::sources::Vector>(&buffer.data)) {
            raw_data = reinterpret_cast<const uint8_t*>(vec_src->bytes.data());
        }

        if (raw_data) {
            const uint8_t* img_data = raw_data + view.byteOffset;

            texture.data = stbi_load_from_memory(
                img_data,
                static_cast<int>(view.byteLength),
                &texture.width, &texture.height, &texture.channels, 4);

            if (texture.data) {
                success = true;
                spdlog::info("Loaded texture: {} ({}x{})", image.name, texture.width, texture.height);
            } else {
                spdlog::error("Failed to decode texture {}: {}", image.name, stbi_failure_reason());
            }
        } else {
            spdlog::error("Failed to get raw data for texture {}", image.name);
        }
    } else {
        spdlog::warn("Image {} is not packed in GLB (skipped)", image.name);
    }

    if (!success) {
        texture = create_placeholder_texture();
    }
    textures.emplace_back(std::move(texture));
}

void Model::process_material(const fastgltf::Asset& asset, const fastgltf::Material& gltf_material) {
    Material material{};

    if (gltf_material.pbrData.baseColorTexture.has_value()) {
        const size_t texture_index = gltf_material.pbrData.baseColorTexture.value().textureIndex;
        const size_t image_index = asset.textures[texture_index].imageIndex.value();
        material.albedo_index = static_cast<uint32_t>(image_index);
    }
    material.base_color_factor = glm::make_vec4(gltf_material.pbrData.baseColorFactor.data());

    if (gltf_material.normalTexture.has_value()) {
        const size_t texture_index = gltf_material.normalTexture.value().textureIndex;
        const size_t image_index = asset.textures[texture_index].imageIndex.value();
        material.normal_index = static_cast<uint32_t>(image_index);
        material.normal_scale = gltf_material.normalTexture.value().scale;
    }

    if (gltf_material.pbrData.metallicRoughnessTexture.has_value()) {
        const size_t texture_index = gltf_material.pbrData.metallicRoughnessTexture.value().textureIndex;
        const size_t image_index = asset.textures[texture_index].imageIndex.value();
        material.metallic_roughness_index = static_cast<uint32_t>(image_index);
    }
    material.metallic_factor = gltf_material.pbrData.metallicFactor;
    material.roughness_factor = gltf_material.pbrData.roughnessFactor;

    if (gltf_material.emissiveTexture.has_value()) {
        const size_t texture_index = gltf_material.emissiveTexture.value().textureIndex;
        const size_t image_index = asset.textures[texture_index].imageIndex.value();
        material.emissive_index = static_cast<uint32_t>(image_index);
    }
    material.emissive_factor = glm::make_vec3(gltf_material.emissiveFactor.data());

    materials.emplace_back(material);
}

Model::Model(const fastgltf::Asset& asset) {
    meshes.reserve(asset.meshes.size());
    size_t prim_count = 0;
    for (const auto& gltf_mesh : asset.meshes) {
        process_mesh(asset, gltf_mesh);
        prim_count += gltf_mesh.primitives.size();
    }

    size_t scene_index = 0;
    if (asset.defaultScene.has_value()) {
        scene_index = asset.defaultScene.value();
    }
    const auto& node_indices = asset.scenes[scene_index].nodeIndices;

    nodes.reserve(node_indices.size());
    for (const size_t node_index : node_indices) {
        process_node(asset, node_index, glm::mat4(1.0f));
    }

    materials.reserve(asset.materials.size());
    for (const auto& material : asset.materials) {
        process_material(asset, material);
    }

    textures.reserve(asset.images.size());
    for (const auto& image : asset.images) {
        process_texture(asset, image);
    }

    spdlog::info("Loaded model with {} meshes, {} primitives, {} nodes, {} materials and {} textures",
                 meshes.size(),
                 prim_count,
                 nodes.size(),
                 materials.size(),
                 textures.size());
}