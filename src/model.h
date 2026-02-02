#pragma once

#include "texture.h"
#include "fastgltf/types.hpp"
#include "glm/glm.hpp"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 texcoord;
};

struct Mesh {
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t vertex_offset;
    uint32_t index_offset;
    uint32_t material_index;
};

struct MeshInstance {
    uint32_t mesh_index;
    glm::mat4 transform;
};

// TODO: alpha_cutoff, alpha_mode
struct Material {
    uint32_t albedo_index = std::numeric_limits<uint32_t>::max();
    glm::vec4 base_color_factor = glm::vec4(1.0f);

    uint32_t normal_index = std::numeric_limits<uint32_t>::max();
    float normal_scale = 1.0f;

    uint32_t metallic_roughness_index = std::numeric_limits<uint32_t>::max();
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;

    uint32_t emissive_index = std::numeric_limits<uint32_t>::max();
    glm::vec3 emissive_factor = glm::vec3(1.0f);
};

class Model {
    void process_mesh(const fastgltf::Asset& asset, const fastgltf::Mesh& gltf_mesh);
    void process_node(const fastgltf::Asset& asset, size_t node_index, const glm::mat4& parent_transform);
    void process_texture(const fastgltf::Asset& asset, const fastgltf::Image& image);
    void process_material(const fastgltf::Asset& asset, const fastgltf::Material& gltf_material);

public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Mesh> meshes;
    std::vector<MeshInstance> mesh_instances;
    std::vector<TextureData> textures;
    std::vector<Material> materials;

    explicit Model(const fastgltf::Asset& asset);
};