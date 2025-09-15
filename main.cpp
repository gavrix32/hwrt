#include <iostream>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <set>
#include <GLFW/glfw3.h>

#define WIDTH 800
#define HEIGHT 600

const std::vector validation_layers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enable_validation_layers = false;
#else
constexpr bool enable_validation_layers = true;
#endif

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                       const vk::DebugUtilsMessageTypeFlagsEXT type,
                                                       const vk::DebugUtilsMessengerCallbackDataEXT *p_callback_data,
                                                       void *) {
    std::cerr << "validation layer: type " << to_string(type) << " msg: " << p_callback_data->pMessage << std::endl;

    return vk::False;
}

std::string_view get_physical_device_type_name(vk::PhysicalDeviceType type) {
    switch (type) {
        case vk::PhysicalDeviceType::eDiscreteGpu: return "discrete";
        case vk::PhysicalDeviceType::eIntegratedGpu: return "integrated";
        case vk::PhysicalDeviceType::eVirtualGpu: return "virtual";
        case vk::PhysicalDeviceType::eCpu: return "cpu";
        default: return "unknown";
    }
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *window = glfwCreateWindow(800, 600, "HWRT", nullptr, nullptr);

    const vk::raii::Context context;

    // Check layers
    std::vector<char const *> required_layers;
    if (enable_validation_layers) {
        required_layers.assign(validation_layers.begin(), validation_layers.end());
    }
    auto available_layers = context.enumerateInstanceLayerProperties();

    for (auto &required_layer: required_layers) {
        bool found = false;
        for (auto &available_layer: available_layers) {
            if (std::strcmp(available_layer.layerName, required_layer) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(
                "Required instance layer not supported: " + std::string(required_layer));
        }
    }

    // Check extensions
    uint32_t required_extension_count = 0;
    const char **required_extensions_data = glfwGetRequiredInstanceExtensions(&required_extension_count);

    std::vector required_extensions(required_extensions_data, required_extensions_data + required_extension_count);

    if (enable_validation_layers) {
        required_extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    auto available_extensions = context.enumerateInstanceExtensionProperties();

    for (auto &required_extension: required_extensions) {
        bool found = false;
        for (auto &available_extension: available_extensions) {
            if (std::strcmp(available_extension.extensionName, required_extension) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(
                "Required GLFW extension not supported: " + std::string(required_extension));
        }
    }

    constexpr vk::ApplicationInfo app_info{
        .pApplicationName = "HWRT",
        .apiVersion = vk::ApiVersion14,
    };

    vk::InstanceCreateInfo instance_create_info{
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = static_cast<uint32_t>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data(),
    };

    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info_EXT;

    if (enable_validation_layers) {
        instance_create_info.enabledLayerCount = static_cast<uint32_t>(required_layers.size());
        instance_create_info.ppEnabledLayerNames = required_layers.data();

        vk::DebugUtilsMessageSeverityFlagsEXT severity_flags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

        vk::DebugUtilsMessageTypeFlagsEXT message_type_flags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

        debug_utils_messenger_create_info_EXT = vk::DebugUtilsMessengerCreateInfoEXT{
            .messageSeverity = severity_flags,
            .messageType = message_type_flags,
            .pfnUserCallback = &debug_callback
        };
    }

    const auto instance = vk::raii::Instance(context, instance_create_info);

    if (enable_validation_layers) {
        vk::raii::DebugUtilsMessengerEXT debug_messenger = instance.createDebugUtilsMessengerEXT(
            debug_utils_messenger_create_info_EXT);
    }

    std::set<std::string> required_device_extension_names;
    required_device_extension_names.insert(vk::KHRRayTracingPipelineExtensionName);
    required_device_extension_names.insert(vk::NVRayTracingExtensionName);

    vk::PhysicalDevice physical_device = nullptr;

    auto physical_devices = instance.enumeratePhysicalDevices();
    if (physical_devices.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    size_t i = 0;
    for (const auto &device: physical_devices) {
        auto properties = device.getProperties();
        auto type = get_physical_device_type_name(properties.deviceType);
        std::cout << "GPU " << i << ": " << properties.deviceName << " (" << type << ")\n";

        auto available_device_extension_properties = device.enumerateDeviceExtensionProperties();

        std::set<std::string> available_device_extension_names;
        for (auto extension_property: available_device_extension_properties) {
            available_device_extension_names.insert(extension_property.extensionName);
        }

        std::set<std::string> missing_device_extension_names;
        for (auto extension_name: required_device_extension_names) {
            if (!available_device_extension_names.contains(extension_name)) {
                missing_device_extension_names.insert(extension_name);
            }
        }

        if (missing_device_extension_names.empty()) {
            std::cout << "    All required extensions supported. Using this device";
            physical_device = device;
            break;
        } else {
            std::cout << "    Required device extensions missing: \n";
            for (auto extension_name: missing_device_extension_names) {
                std::cout << "        - " << extension_name << "\n";
            }
        }
        std::cout << "\n";

        ++i;
    }
    if (physical_device == nullptr) {
        throw std::runtime_error("Failed to find a physical device with support for all required extensions");
    }

    // std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_device.getQueueFamilyProperties();

    // vk::DeviceQueueCreateInfo device_queue_create_info { .queueFamilyIndex = graphicsIndex };

    // while (!glfwWindowShouldClose(window)) {
    //     glfwPollEvents();
    // }
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
