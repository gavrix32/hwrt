#pragma once

#include "camera.h"
#include "model.h"

// TODO: Instancing

struct ModelInstance {
    Model& model;
    glm::mat4 transform;
};

class Scene {
    Camera camera;
    std::vector<ModelInstance> model_instances;
    AccelerationStructure tlas;

public:
    Scene() = default;

    void set_camera(const Camera& camera_) {
        this->camera = camera_;
    }

    [[nodiscard]] const Camera& get_camera() const {
        return camera;
    }

    void add_instance(Model& model, const glm::mat4& transform) {
        model_instances.push_back({model, transform});
    }

    void build_tlas(const Context& ctx);

    [[nodiscard]] const AccelerationStructure& get_tlas() const {
        return tlas;
    }
};