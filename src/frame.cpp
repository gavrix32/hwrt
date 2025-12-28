#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#include "vulkan/device.h"
#include "frame.h"

FrameManager::FrameManager(const Device& device, const int frames_in_flight, const int swapchain_image_count)
    : frames_in_flight(frames_in_flight), swapchain_image_count(swapchain_image_count) {
    in_flight_fences.reserve(frames_in_flight);
    image_available_semaphores.reserve(frames_in_flight);
    render_finished_semaphores.reserve(swapchain_image_count);

    for (int i = 0; i < frames_in_flight; ++i) {
        vk::FenceCreateInfo fence_create_info{
            .flags = vk::FenceCreateFlagBits::eSignaled
        };
        in_flight_fences.emplace_back(device.get(), fence_create_info);
        image_available_semaphores.emplace_back(device.get(), vk::SemaphoreCreateInfo{});
    }
    for (int i = 0; i < swapchain_image_count; ++i) {
        render_finished_semaphores.emplace_back(device.get(), vk::SemaphoreCreateInfo{});
    }
}

int FrameManager::get_frame_index() const {
    return current_frame;
}

void FrameManager::update() {
    current_frame = (current_frame + 1) % frames_in_flight;
}

const vk::raii::Fence& FrameManager::get_in_flight_fence() const {
    return in_flight_fences[current_frame];
}

const vk::raii::Semaphore& FrameManager::get_image_available_semaphore() const {
    return image_available_semaphores[current_frame];
}
const vk::raii::Semaphore& FrameManager::get_render_finished_semaphore(const int image_index) const {
    return render_finished_semaphores[image_index];
}
