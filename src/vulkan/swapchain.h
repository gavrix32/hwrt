#pragma once

class Instance;
class Adapter;
class Device;
class GLFWwindow;

class Swapchain {
    vk::Extent2D extent;
    vk::SurfaceFormatKHR format;
    vk::raii::SurfaceKHR surface_khr;
    vk::raii::SwapchainKHR handle;

public:
    explicit Swapchain(const Instance& instance, const Adapter& adapter, const Device& device, GLFWwindow* window);
    [[nodiscard]] vk::Extent2D get_extent() const;
    [[nodiscard]] vk::SurfaceFormatKHR get_surface_format() const;
    [[nodiscard]] std::vector<vk::Image> get_images() const;
    [[nodiscard]] const vk::raii::SwapchainKHR& get() const;
};