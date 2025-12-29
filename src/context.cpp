#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>

#include "context.h"

std::vector device_extensions = {
    vk::KHRSwapchainExtensionName,
    vk::KHRDeferredHostOperationsExtensionName,
    vk::KHRAccelerationStructureExtensionName,
    vk::KHRRayTracingPipelineExtensionName
};

Context::Context(GLFWwindow* window, const bool validation)
    : instance(validation),
      adapter(instance, device_extensions),
      device(adapter, device_extensions),
      swapchain(instance, adapter, device, window),
      allocator(instance, adapter, device) {
}

const Instance& Context::get_instance() const {
    return instance;
}

const Adapter& Context::get_adapter() const {
    return adapter;
}

const Device& Context::get_device() const {
    return device;
}

const Swapchain& Context::get_swapchain() const {
    return swapchain;
}

const Allocator& Context::get_allocator() const {
    return allocator;
}