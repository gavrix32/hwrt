#include <vulkan/vulkan_raii.hpp>

#include "context.h"

std::vector device_extensions = {
    vk::KHRSwapchainExtensionName,
    vk::KHRDeferredHostOperationsExtensionName,
    vk::KHRAccelerationStructureExtensionName,
    vk::KHRRayTracingPipelineExtensionName,
    vk::KHRShaderClockExtensionName,
};

Context::Context(const bool validation)
    : instance(validation),
      adapter(instance, device_extensions),
      device(adapter, device_extensions),
      allocator(instance, adapter, device),
      linear_sampler(device, vk::Filter::eLinear, vk::Filter::eLinear),
      bindless_layout(nullptr) {
    std::vector<vk::DescriptorSetLayoutBinding> descriptor_bindings;

    constexpr vk::DescriptorSetLayoutBinding bindless_binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = MAX_TEXTURES,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    };
    descriptor_bindings.push_back(bindless_binding);

    auto binding_flags =
        vk::DescriptorBindingFlagBits::ePartiallyBound |
        vk::DescriptorBindingFlagBits::eUpdateAfterBind;

    vk::DescriptorSetLayoutBindingFlagsCreateInfo flags_info = {
        .bindingCount = 1,
        .pBindingFlags = &binding_flags
    };

    const vk::DescriptorSetLayoutCreateInfo layout_create_info{
        .pNext = &flags_info,
        .flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
        .bindingCount = static_cast<uint32_t>(descriptor_bindings.size()),
        .pBindings = descriptor_bindings.data(),
    };
    bindless_layout = device.get().createDescriptorSetLayout(layout_create_info);
}