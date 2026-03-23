#pragma once

#include "camera.h"
#include "context.h"
#include "model.h"

#include "vulkan/acceleration.h"
#include "vulkan/image.h"

struct ModelInstance {
    std::shared_ptr<Model> model;
    glm::mat4 transform;
    uint32_t first_blas;
};

struct Blas {
    AccelerationStructure as;
    uint32_t geometry_offset;
    uint32_t geometry_count;
};

class Scene {
    Camera camera;

    std::unordered_map<Model*, uint32_t> model_cache;

    std::vector<ModelInstance> model_instances;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Material> materials;
    std::vector<Geometry> geometries;

    Buffer vertex_buffer;
    Buffer index_buffer;
    Buffer material_buffer;
    Buffer geometry_buffer;

    std::vector<Image> images;
    std::vector<ImageView> image_views;

    vk::raii::DescriptorPool descriptor_pool = nullptr;
    vk::raii::DescriptorSet descriptor_set = nullptr;

    ScenePtrs scene_ptrs{};

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

    void add_instance(const std::shared_ptr<Model>& model, const glm::mat4& transform, const Context& ctx);

    void build_blases(const Context& ctx);
    void build_tlas(const Context& ctx);
    void build_descriptor_set(const Context& ctx);

    [[nodiscard]] const AccelerationStructure& get_tlas() const {
        return tlas;
    }

    [[nodiscard]] const std::vector<Image>& get_images() const {
        return images;
    }

    [[nodiscard]] const std::vector<ModelInstance>& get_instances() const {
        return model_instances;
    }

    [[nodiscard]] const ScenePtrs& get_scene_ptrs() const {
        return scene_ptrs;
    }

    [[nodiscard]] const vk::raii::DescriptorSet& get_descriptor_set() const {
        return descriptor_set;
    }
};