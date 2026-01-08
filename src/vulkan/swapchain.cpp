#include <GLFW/glfw3.h>

#include "swapchain.h"
#include "instance.h"
#include "adapter.h"
#include "device.h"
#include "image.h"
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

Swapchain::Swapchain(const Adapter& adapter, const Device& device,
                     GLFWwindow* window, const vk::SurfaceKHR& surface)
    : handle(nullptr) {
    SCOPED_TIMER();

    const auto surface_capabilities = adapter.get().getSurfaceCapabilitiesKHR(surface);

    extent = choose_extent(window, surface_capabilities);
    format = choose_format(adapter.get().getSurfaceFormatsKHR(surface));

    const vk::SwapchainCreateInfoKHR swapchain_create_info{
        .surface = surface,
        .minImageCount = surface_capabilities.minImageCount,
        .imageFormat = format.format,
        .imageColorSpace = format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eTransferDst,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = choose_present_mode(adapter.get().getSurfacePresentModesKHR(surface)),
        .clipped = true,
    };

    handle = vk::raii::SwapchainKHR(device.get(), swapchain_create_info);

    const auto image_handles = handle.getImages();

    images.reserve(image_handles.size());
    for (auto image_handle : image_handles) {
        images.emplace_back(image_handle);
    }
}

vk::Extent2D Swapchain::get_extent() const {
    return extent;
}

vk::SurfaceFormatKHR Swapchain::get_surface_format() const {
    return format;
}

std::vector<Image>& Swapchain::get_images() {
    return images;
}

const vk::raii::SwapchainKHR& Swapchain::get() const {
    return handle;
}