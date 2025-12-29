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

vk::SurfaceFormatKHR choose_format(const std::vector<vk::SurfaceFormatKHR>& available_formats) {
    for (const auto& available_format : available_formats) {
        if (available_format.format == vk::Format::eB8G8R8A8Srgb &&
            available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return available_format;
        }
    }
    return available_formats[0];
}

vk::PresentModeKHR choose_present_mode(const std::vector<vk::PresentModeKHR>& available_present_modes) {
    for (const auto& available_present_mode : available_present_modes) {
        if (available_present_mode == vk::PresentModeKHR::eImmediate) {
            return available_present_mode;
        }
    }
    return available_present_modes[0];
}

Swapchain::Swapchain(const Instance& instance, const Adapter& adapter, const Device& device,
                     GLFWwindow* window) : surface_khr(nullptr), handle(nullptr) {
    SCOPED_TIMER();

    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance.get(), window, nullptr, &_surface) != 0) {
        throw std::runtime_error("Failed to create window surface");
    }
    surface_khr = vk::raii::SurfaceKHR(instance.get(), _surface);

    const auto surface_capabilities = adapter.get().getSurfaceCapabilitiesKHR(*surface_khr);

    extent = choose_extent(window, surface_capabilities);
    format = choose_format(adapter.get().getSurfaceFormatsKHR(*surface_khr));

    const vk::SwapchainCreateInfoKHR swapchain_create_info{
        .surface = *surface_khr,
        .minImageCount = surface_capabilities.minImageCount,
        .imageFormat = format.format,
        .imageColorSpace = format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eTransferDst,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = choose_present_mode(adapter.get().getSurfacePresentModesKHR(*surface_khr)),
        .clipped = true
    };

    handle = vk::raii::SwapchainKHR(device.get(), swapchain_create_info);
}

vk::Extent2D Swapchain::get_extent() const {
    return extent;
}

vk::SurfaceFormatKHR Swapchain::get_surface_format() const {
    return format;
}

std::vector<vk::Image> Swapchain::get_images() const {
    return handle.getImages();
}

const vk::raii::SwapchainKHR& Swapchain::get() const {
    return handle;
}