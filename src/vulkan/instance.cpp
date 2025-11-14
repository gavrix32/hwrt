#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "instance.h"

Instance::Instance(bool validation) : vk_instance(nullptr) {

    const std::vector validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

    std::vector<char const*> required_layers;
    if (validation) {
        required_layers.assign(validation_layers.begin(), validation_layers.end());
    }
    auto available_layers = vk_context.enumerateInstanceLayerProperties();

    for (auto& required_layer : required_layers) {
        bool found = false;
        for (auto& available_layer : available_layers) {
            if (std::strcmp(available_layer.layerName, required_layer) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error("Required instance layer not supported: " + std::string(required_layer));
        }
    }

    uint32_t required_extension_count = 0;
    const char** required_instance_extensions_data = glfwGetRequiredInstanceExtensions(&required_extension_count);

    std::vector required_instance_extensions(required_instance_extensions_data,
                                             required_instance_extensions_data + required_extension_count);

    if (validation) {
        required_instance_extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    auto available_extensions = vk_context.enumerateInstanceExtensionProperties();

    for (auto& extension : required_instance_extensions) {
        bool found = false;
        for (auto& available_extension : available_extensions) {
            if (std::strcmp(available_extension.extensionName, extension) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error("Required extension not supported: " + std::string(extension));
        }
    }

    constexpr vk::ApplicationInfo app_info{
        .pApplicationName = "HWRT",
        .apiVersion = vk::ApiVersion14,
    };

    std::vector validation_features_enabled_list{
        vk::ValidationFeatureEnableEXT::eBestPractices,
        vk::ValidationFeatureEnableEXT::eSynchronizationValidation,
        // vk::ValidationFeatureEnableEXT::eGpuAssisted,
        // vk::ValidationFeatureEnableEXT::eGpuAssistedReserveBindingSlot,
        // vk::ValidationFeatureEnableEXT::eDebugPrintf,
    };

    vk::ValidationFeaturesEXT validation_features{
        .enabledValidationFeatureCount = static_cast<uint32_t>(validation_features_enabled_list.size()),
        .pEnabledValidationFeatures = validation_features_enabled_list.data(),
    };

    vk::InstanceCreateInfo instance_create_info{
        .pNext = validation_features,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = static_cast<uint32_t>(required_instance_extensions.size()),
        .ppEnabledExtensionNames = required_instance_extensions.data(),
    };

    if (validation) {
        instance_create_info.enabledLayerCount = static_cast<uint32_t>(required_layers.size());
        instance_create_info.ppEnabledLayerNames = required_layers.data();
    }

    vk_instance = vk::raii::Instance(vk_context, instance_create_info);
}

const vk::raii::Instance& Instance::get() const {
    return vk_instance;
}