#pragma once

#include "tiny_gltf.h"
#include "glm/glm.hpp"

#include "vulkan/acceleration.h"
#include "vulkan/buffer.h"

class Device;
class Allocator;
class Context;

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 texcoord;
};

struct Mesh {
    uint32_t first_index;
    uint32_t index_count;
    uint32_t vertex_offset;
};

struct MeshInstance {
    uint32_t mesh_index;
    glm::mat4 transform;
};

struct alignas(8) GpuMesh {
    uint64_t vertex_address;
    uint64_t index_address;
    uint32_t first_index;
    uint32_t material_index;
};

class Model {
    Buffer vertex_buffer;
    Buffer index_buffer;

    void process_node(const tinygltf::Model& gltf_model, int node_index, const glm::mat4& parent_transform);

    void create_vertex_buffer(const Allocator& allocator, uint32_t vertex_count);
    void create_index_buffer(const Allocator& allocator, uint32_t index_count);

    void build_blases(const Context& ctx,
                      vk::DeviceAddress vertex_address,
                      vk::DeviceAddress index_address,
                      std::vector<Mesh>& meshes,
                      uint32_t max_vertex);

public:
    std::vector<AccelerationStructure> blases;
    std::vector<MeshInstance> mesh_instances;
    std::vector<GpuMesh> gpu_meshes;

    vk::DeviceAddress vertex_address;
    vk::DeviceAddress index_address;

    explicit Model(const Context& ctx, const tinygltf::Model& gltf_model);
};