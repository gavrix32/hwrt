#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "glm/glm.hpp"

#include "vulkan/buffer.h"

class Context;

struct Uniform {
    // std140 base alignment 16 bytes
    glm::mat4 inv_view; // 0
    glm::mat4 inv_proj; // 64
    // ============= 128
};

class FrameManager {
    int current_frame = 0;
    int frames_in_flight;
    int swapchain_image_count;

    std::vector<vk::raii::Fence> in_flight_fences;
    std::vector<vk::raii::Semaphore> image_available_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;
    std::vector<Buffer> uniform_buffers;

public:
    explicit FrameManager(const Context& ctx, int frames_in_flight, int swapchain_image_count);

    void update();
    void recreate_image_available_semaphores(const Device& device);

    [[nodiscard]] int get_frame_index() const;
    [[nodiscard]] const vk::raii::Fence& get_in_flight_fence() const;
    [[nodiscard]] const vk::raii::Semaphore& get_image_available_semaphore() const;
    [[nodiscard]] const vk::raii::Semaphore& get_render_finished_semaphore(uint32_t image_index) const;
    [[nodiscard]] const Buffer& get_uniform_buffer() const;
};