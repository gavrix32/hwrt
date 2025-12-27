#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>

#include "context.h"

Context::Context(GLFWwindow* window, bool validation) {
    instance.emplace(validation);

    std::vector device_extensions = {
        vk::KHRSwapchainExtensionName,
        vk::KHRDeferredHostOperationsExtensionName,
        vk::KHRAccelerationStructureExtensionName,
        vk::KHRRayTracingPipelineExtensionName
    };
    adapter.emplace(*instance, device_extensions);

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceVulkan14Features,
                       vk::PhysicalDeviceBufferDeviceAddressFeatures, vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
                       vk::PhysicalDeviceRayTracingPipelineFeaturesKHR> features_chain;

    features_chain.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = vk::True;
    features_chain.get<vk::PhysicalDeviceVulkan14Features>().pushDescriptor = vk::True;
    features_chain.get<vk::PhysicalDeviceBufferDeviceAddressFeatures>().bufferDeviceAddress = vk::True;
    features_chain.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>().accelerationStructure = vk::True;
    features_chain.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>().rayTracingPipeline = vk::True;

    device.emplace(*adapter, device_extensions, features_chain.get<vk::PhysicalDeviceFeatures2>());
    swapchain.emplace(*instance, *adapter, *device, window);
    allocator.emplace(*instance, *adapter, *device);
}

const Instance& Context::get_instance() const {
    return *instance;
}

const Adapter& Context::get_adapter() const {
    return *adapter;
}

const Device& Context::get_device() const {
    return *device;
}

const Swapchain& Context::get_swapchain() const {
    return *swapchain;
}

const Allocator& Context::get_allocator() const {
    return *allocator;
}