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

struct Primitive {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t material_index;
};

struct Mesh {
    std::vector<Primitive> primitives;
};

struct Node {
    uint32_t mesh_index;
    glm::mat4 transform;
};

// TODO: alpha_cutoff, alpha_mode
struct Material {
    uint32_t albedo_index;
    glm::vec4 base_color_factor;

    uint32_t normal_index;
    float normal_scale;

    uint32_t metallic_roughness_index;
    float metallic_factor;
    float roughness_factor;

    uint32_t emissive_index;
    glm::vec3 emissive_factor;
};

class Model {
    void process_mesh(const fastgltf::Asset& asset, const fastgltf::Mesh& gltf_mesh);
    void process_node(const fastgltf::Asset& asset, size_t node_index, const glm::mat4& parent_transform);
    void process_material(const fastgltf::Asset& asset, const fastgltf::Material& gltf_material);
    void process_texture(const fastgltf::Asset& asset, const fastgltf::Image& image);

public:
    std::vector<Mesh> meshes;
    std::vector<Node> nodes;
    std::vector<Material> materials;
    std::vector<TextureData> textures;

    explicit Model(const fastgltf::Asset& asset);
};