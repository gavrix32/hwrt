#pragma once

#include "texture.h"

#include <fastgltf/types.hpp>
#include <glm/glm.hpp>

#include "common.h"

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