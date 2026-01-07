#pragma once

#include <vulkan/vulkan_raii.hpp>

class GLFWwindow;

class Instance;
class Adapter;
class Device;
class Image;

class Swapchain {
    vk::Extent2D extent;
    vk::SurfaceFormatKHR format;
    vk::raii::SurfaceKHR surface_khr;
    vk::raii::SwapchainKHR handle;
    std::vector<Image> images;

public:
    explicit Swapchain(const Instance& instance, const Adapter& adapter, const Device& device, GLFWwindow* window);
    [[nodiscard]] vk::Extent2D get_extent() const;
    [[nodiscard]] vk::SurfaceFormatKHR get_surface_format() const;
    [[nodiscard]] std::vector<Image>& get_images();
    [[nodiscard]] const vk::raii::SwapchainKHR& get() const;
};