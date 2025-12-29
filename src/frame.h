#pragma once

#include <vulkan/vulkan_raii.hpp>

class Device;

class FrameManager {
    int current_frame = 0;
    int frames_in_flight;
    int swapchain_image_count;

    std::vector<vk::raii::Fence> in_flight_fences;
    std::vector<vk::raii::Semaphore> image_available_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;

public:
    explicit FrameManager(const Device& device, int frames_in_flight, int swapchain_image_count);

    void update();

    [[nodiscard]] int get_frame_index() const;
    [[nodiscard]] const vk::raii::Fence& get_in_flight_fence() const;
    [[nodiscard]] const vk::raii::Semaphore& get_image_available_semaphore() const;
    [[nodiscard]] const vk::raii::Semaphore& get_render_finished_semaphore(uint32_t image_index) const;
};