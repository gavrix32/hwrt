#include <set>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#include <spdlog/spdlog.h>

#include "instance.h"
#include "adapter.h"
#include "utils.h"

std::string_view get_physical_device_type_name(const vk::PhysicalDeviceType type) {
    switch (type) {
        case vk::PhysicalDeviceType::eDiscreteGpu: return "discrete";
        case vk::PhysicalDeviceType::eIntegratedGpu: return "integrated";
        case vk::PhysicalDeviceType::eVirtualGpu: return "virtual";
        case vk::PhysicalDeviceType::eCpu: return "cpu";
        default: return "unknown";
    }
}

Adapter::Adapter(const Instance& instance, const std::vector<const char*>& required_extensions) : vk_physical_device(nullptr) {
    SCOPED_TIMER_NAMED("Created VkPhysicalDevice");

    const auto physical_devices = instance.get().enumeratePhysicalDevices();
    if (physical_devices.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    // Should be in function
    size_t device_idx = 0;
    for (const auto& physical_device : physical_devices) {
        auto properties = physical_device.getProperties();
        auto available_device_extension_properties = physical_device.enumerateDeviceExtensionProperties();

        auto type = get_physical_device_type_name(properties.deviceType);
        spdlog::info("GPU {}: {} ({})", device_idx, properties.deviceName.data(), type);

        std::set<std::string> available_device_extension_names;
        for (auto extension_property : available_device_extension_properties) {
            available_device_extension_names.insert(extension_property.extensionName);
        }

        std::set<std::string> missing_device_extension_names;
        for (const auto& extension_name : required_extensions) {
            if (!available_device_extension_names.contains(extension_name)) {
                missing_device_extension_names.insert(extension_name);
            }
        }

        if (missing_device_extension_names.empty()) {
            spdlog::info("    All required extensions supported. Using this device");
            vk_physical_device = physical_device;
            break;
        } else {
            spdlog::error("    Required device extensions missing: ");
            for (const auto& extension_name : missing_device_extension_names) {
                spdlog::error("        - {}", extension_name);
            }
        }

        ++device_idx;
    }
    if (vk_physical_device == nullptr) {
        spdlog::critical("Failed to find a physical device with support for all required extensions");
    }
}

const vk::raii::PhysicalDevice& Adapter::get() const {
    return vk_physical_device;
}