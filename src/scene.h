#pragma once

#include "camera.h"
#include "context.h"
#include "model.h"
#include "vulkan/acceleration.h"

struct ModelInstance {
    std::shared_ptr<Model> model;
    glm::mat4 transform;
    uint32_t first_blas;
};

struct Geometry {
    uint32_t vertex_offset;
    uint32_t vertex_count;
    uint32_t index_offset;
    uint32_t index_count;
    uint32_t material_index;
};

struct SceneAddresses {
    uint64_t vertex_address;
    uint64_t index_address;
    uint64_t material_address;
    uint64_t geometry_address;
};

struct Blas {
    AccelerationStructure as;
    uint32_t geometry_offset;
    uint32_t geometry_count;
};

class Scene {
    Camera camera;

    std::vector<ModelInstance> model_instances;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Material> materials;
    std::vector<Geometry> geometries;

    Buffer vertex_buffer;
    Buffer index_buffer;
    Buffer material_buffer;
    Buffer geometry_buffer;

    SceneAddresses scene_addresses;

    std::vector<Blas> blases;
    AccelerationStructure tlas;

public:
    Scene() = default;

    void set_camera(const Camera& camera_) {
        this->camera = camera_;
    }

    [[nodiscard]] const Camera& get_camera() const {
        return camera;
    }

    void add_instance(const std::shared_ptr<Model>& model, const glm::mat4& transform);

    void build_blases(const Context& ctx);
    void build_tlas(const Context& ctx);

    [[nodiscard]] const AccelerationStructure& get_tlas() const {
        return tlas;
    }

    [[nodiscard]] const SceneAddresses& get_scene_address() const {
        return scene_addresses;
    }
};