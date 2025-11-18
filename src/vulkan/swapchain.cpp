#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "swapchain.h"
#include "instance.h"
#include "adapter.h"
#include "device.h"
#include "utils.h"

vk::Extent2D choose_extent(GLFWwindow* window, const vk::SurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    auto min_extent = capabilities.minImageExtent;
    auto max_extent = capabilities.maxImageExtent;

    return {
        .width = std::clamp<uint32_t>(width, min_extent.width, max_extent.width),
        .height = std::clamp<uint32_t>(height, min_extent.height, max_extent.height),
    };
}

Swapchain::Swapchain(const Instance& instance, const Adapter& adapter, const Device& device,
                     GLFWwindow* window) : vk_swapchain_khr(nullptr), vk_surface_khr(nullptr) {
    SCOPED_TIMER_NAMED("Create VkSurfaceKHR, VkSwapchainKHR");

    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance.get(), window, nullptr, &_surface) != 0) {
        throw std::runtime_error("Failed to create window surface");
    }
    vk_surface_khr = vk::raii::SurfaceKHR(instance.get(), _surface);

    const auto surface_capabilities = adapter.get().getSurfaceCapabilitiesKHR(*vk_surface_khr);
    vk_extent = choose_extent(window, surface_capabilities);

    auto available_formats = adapter.get().getSurfaceFormatsKHR(*vk_surface_khr);
    for (const auto& available_format : available_formats) {
        if (available_format.format == vk::Format::eB8G8R8A8Srgb &&
            available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            vk_surface_format = available_format;
            break;
        }
        vk_surface_format = available_formats[0];
    }

    const vk::SwapchainCreateInfoKHR swapchain_create_info{
        .surface = *vk_surface_khr,
        .minImageCount = surface_capabilities.minImageCount,
        .imageFormat = vk_surface_format.format,
        .imageColorSpace = vk_surface_format.colorSpace,
        .imageExtent = vk_extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eTransferDst,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = vk::PresentModeKHR::eImmediate,
        .clipped = true
    };

    vk_swapchain_khr = vk::raii::SwapchainKHR(device.get(), swapchain_create_info);
}

vk::Extent2D Swapchain::get_extent() const {
    return vk_extent;
}

vk::SurfaceFormatKHR Swapchain::get_surface_format() const {
    return vk_surface_format;
}

std::vector<vk::Image> Swapchain::get_images() const {
    return vk_swapchain_khr.getImages();
}

const vk::raii::SwapchainKHR& Swapchain::get() const {
    return vk_swapchain_khr;
}