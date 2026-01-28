#pragma once

#include "tiny_gltf.h"
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
};

struct MeshInstance {
    uint32_t mesh_index;
    glm::mat4 transform;
};

class Model {
    void process_mesh(const tinygltf::Model& gltf_model, const tinygltf::Mesh& gltf_mesh);
    void process_node(const tinygltf::Model& gltf_model, int node_index, const glm::mat4& parent_transform);

public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Mesh> meshes;
    std::vector<MeshInstance> mesh_instances;

    explicit Model(const tinygltf::Model& gltf_model);
};