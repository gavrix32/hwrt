#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "image.h"

class GLFWwindow;

class Instance;
class Adapter;
class Device;

class Swapchain {
    vk::Extent2D extent;
    vk::SurfaceFormatKHR format;
    vk::raii::SwapchainKHR handle;
    std::vector<Image> images;
    std::vector<ImageView> image_views;

public:
    explicit Swapchain(const Adapter& adapter,
                       const Device& device, GLFWwindow* window,
                       const vk::SurfaceKHR& surface,
                       const vk::raii::SwapchainKHR& old_handle);

    [[nodiscard]] vk::Extent2D get_extent() const {
        return extent;
    }

    [[nodiscard]] vk::SurfaceFormatKHR get_surface_format() const {
        return format;
    }

    [[nodiscard]] vk::Format get_format() const {
        return format.format;
    }

    [[nodiscard]] std::vector<Image>& get_images() {
        return images;
    }

    [[nodiscard]] std::vector<ImageView>& get_image_views() {
        return image_views;
    }

    [[nodiscard]] uint32_t get_image_count() const {
        return images.size();
    }

    [[nodiscard]] const vk::raii::SwapchainKHR& get() const {
        return handle;
    }
};