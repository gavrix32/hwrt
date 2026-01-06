#include "camera.h"

#include "window.h"

Camera::Camera() {
    proj = glm::perspective(glm::radians(fov), Window::get_aspect_ratio(), 0.001f, 1000.0f);
}

glm::vec3 Camera::get_pos() const {
    return pos;
}

glm::mat4 Camera::get_view() const {
    const glm::mat4 rotation = glm::mat4_cast(glm::conjugate(quat));
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), -pos);
    return rotation * translation;
}

glm::mat4 Camera::get_proj() const {
    return proj;
}

void Camera::set_pos(const glm::vec3 new_pos) {
    pos = new_pos;
}

void Camera::set_rot(const glm::vec2 new_rot) {
    pitch = new_rot.x;
    yaw = new_rot.y;

    const glm::quat q_yaw = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat q_pitch = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    quat = q_yaw * q_pitch;
}

void Camera::set_fov(const float new_fov) {
    fov = new_fov;
    proj = glm::perspective(glm::radians(fov), Window::get_aspect_ratio(), 0.001f, 1000.0f);
}

glm::vec2 Camera::get_rot() const {
    return glm::vec2(pitch, yaw);
}

void Camera::move(const float x, const float y, const float z) {
    const glm::vec3 right = quat * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 up = quat * glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 forward = quat * glm::vec3(0.0f, 0.0f, -1.0f);

    pos += right * x + up * y + forward * z;
}

void Camera::move_x(const float speed) {
    move(speed, 0.0f, 0.0f);
}

void Camera::move_y(const float speed) {
    move(0.0f, speed, 0.0f);
}

void Camera::move_z(const float speed) {
    move(0.0f, 0.0f, -speed);
}