#pragma once

class Instance;
class Adapter;
class Device;
class GLFWwindow;

class Swapchain {
    vk::Extent2D vk_extent;
    vk::SurfaceFormatKHR vk_surface_format;
    vk::raii::SurfaceKHR vk_surface_khr;
    vk::raii::SwapchainKHR vk_swapchain_khr;

public:
    explicit Swapchain(const Instance& instance, const Adapter& adapter, const Device& device, GLFWwindow* window);
    vk::Extent2D get_extent() const;
    vk::SurfaceFormatKHR get_surface_format() const;
    std::vector<vk::Image> get_images() const;
    const vk::raii::SwapchainKHR& get() const;
};