#pragma once

#include "camera.h"
#include "context.h"
#include "model.h"
#include "vulkan/acceleration.h"

struct ModelInfo {
    uint32_t id;
    uint32_t first_blas_id;
    uint32_t global_vertex_offset;
    uint32_t global_index_offset;
    uint32_t vertex_count;
    uint32_t index_count;
};

struct ModelInstance {
    const Model& model;
    glm::mat4 transform;
    ModelInfo info;
};

// struct alignas(8) GpuMesh {
//     uint64_t vertex_address;
//     uint64_t index_address;
//     uint32_t index_offset;
//     uint32_t material_index;
// };

class Scene {
    std::unordered_map<const Model*, ModelInfo> unique_models;

    Camera camera;
    std::vector<ModelInstance> model_instances;

    std::vector<Vertex> global_vertices;
    std::vector<uint32_t> global_indices;

    Buffer global_vertex_buffer;
    Buffer global_index_buffer;

    uint32_t blas_count = 0;
    std::vector<AccelerationStructure> blases;

    AccelerationStructure tlas;

public:
    Scene() = default;

    void set_camera(const Camera& camera_) {
        this->camera = camera_;
    }

    [[nodiscard]] const Camera& get_camera() const {
        return camera;
    }

    void add_instance(const Model& model, const glm::mat4& transform);
    void build_blases(const Context& ctx);
    void build_tlas(const Context& ctx);

    [[nodiscard]] const AccelerationStructure& get_tlas() const {
        return tlas;
    }
};