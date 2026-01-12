#pragma once

#include "glm/glm.hpp"
#include <glm/gtc/quaternion.hpp>

class Camera {
    glm::vec3 pos = glm::vec3(0.0f);
    glm::quat quat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    float pitch = 0.0f;
    float yaw = 0.0f;
    float fov = 70.0f;

public:
    explicit Camera();

    [[nodiscard]] glm::vec3 get_pos() const;
    [[nodiscard]] glm::mat4 get_view() const;
    [[nodiscard]] glm::mat4 get_proj() const;

    void set_pos(glm::vec3 new_pos);
    void set_rot(glm::vec2 new_rot);
    void set_fov(float new_fov);

    [[nodiscard]] glm::vec2 get_rot() const;

    void move(float x, float y, float z);
    void move_x(float speed);
    void move_y(float speed);
    void move_z(float speed);
};