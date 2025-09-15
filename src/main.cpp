#include <iostream>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>
#include <spdlog/spdlog.h>

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

static spdlog::level::level_enum to_spdlog_level(const vk::DebugUtilsMessageSeverityFlagBitsEXT severity) {
    if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) return spdlog::level::err;
    if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) return spdlog::level::warn;
    if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) return spdlog::level::info;
    return spdlog::level::debug;
}

static std::string_view message_type_to_string(vk::DebugUtilsMessageTypeFlagsEXT type) {
    if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation) return "VALIDATION";
    if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance) return "PERFORMANCE";
    if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral) return "GENERAL";
    return "UNKNOWN";
}

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                       vk::DebugUtilsMessageTypeFlagsEXT type,
                                                       const vk::DebugUtilsMessengerCallbackDataEXT *p_callback_data,
                                                       void *) {
    spdlog::log(to_spdlog_level(severity), "[Vulkan {}] {}", message_type_to_string(type), p_callback_data->pMessage);

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
#ifdef NDEBUG
    spdlog::set_level(spdlog::level::warn);
#else
    spdlog::set_level(spdlog::level::trace);
#endif

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

    if (enable_validation_layers) {
        instance_create_info.enabledLayerCount = static_cast<uint32_t>(required_layers.size());
        instance_create_info.ppEnabledLayerNames = required_layers.data();
    }

    const auto instance = vk::raii::Instance(context, instance_create_info);

    vk::raii::DebugUtilsMessengerEXT debug_messenger{nullptr};

    if (enable_validation_layers) {
        vk::DebugUtilsMessageSeverityFlagsEXT severity_flags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

        vk::DebugUtilsMessageTypeFlagsEXT message_type_flags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

        auto debug_utils_messenger_create_info_EXT = vk::DebugUtilsMessengerCreateInfoEXT{
            .messageSeverity = severity_flags,
            .messageType = message_type_flags,
            .pfnUserCallback = &debug_callback
        };

        debug_messenger = instance.createDebugUtilsMessengerEXT(
            debug_utils_messenger_create_info_EXT);
    }

    std::set<std::string> required_device_extension_names;
    required_device_extension_names.insert(vk::KHRDynamicRenderingExtensionName);
    required_device_extension_names.insert(vk::KHRRayTracingPipelineExtensionName);

    vk::PhysicalDevice adapter = nullptr;

    auto physical_devices = instance.enumeratePhysicalDevices();
    if (physical_devices.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    // Should be in function
    size_t i = 0;
    for (const auto &physical_device: physical_devices) {
        auto properties = physical_device.getProperties();
        auto available_device_extension_properties = physical_device.enumerateDeviceExtensionProperties();

        auto type = get_physical_device_type_name(properties.deviceType);
        spdlog::info("GPU {}: {} ({})", i, properties.deviceName.data(), type);

        std::set<std::string> available_device_extension_names;
        for (auto extension_property: available_device_extension_properties) {
            available_device_extension_names.insert(extension_property.extensionName);
        }

        std::set<std::string> missing_device_extension_names;
        for (const auto &extension_name: required_device_extension_names) {
            if (!available_device_extension_names.contains(extension_name)) {
                missing_device_extension_names.insert(extension_name);
            }
        }

        if (missing_device_extension_names.empty()) {
            spdlog::info("    All required extensions supported. Using this device");
            adapter = physical_device;
            break;
        } else {
            spdlog::info("    Required device extensions missing: ");
            for (const auto &extension_name: missing_device_extension_names) {
                spdlog::info("        - {}", extension_name);
            }
        }

        ++i;
    }
    if (adapter == nullptr) {
        spdlog::critical("Failed to find a physical device with support for all required extensions");
    }
    // ................

    auto queue_family_properties = adapter.getQueueFamilyProperties();

    size_t queue_family_index = 0;

    i = 0;
    for (auto queue_family_property: queue_family_properties) {
        std::string flag_names;

        auto flags = queue_family_property.queueFlags;
        if (flags & vk::QueueFlagBits::eGraphics) flag_names += "Graphics | ";
        if (flags & vk::QueueFlagBits::eCompute) flag_names += "Compute | ";
        if (flags & vk::QueueFlagBits::eTransfer) flag_names += "Transfer | ";
        if (flags & vk::QueueFlagBits::eSparseBinding) flag_names += "SparseBinding | ";
        if (flags & vk::QueueFlagBits::eProtected) flag_names += "Protected | ";
        if (flags & vk::QueueFlagBits::eVideoDecodeKHR) flag_names += "VideoDecodeKHR | ";
        if (flags & vk::QueueFlagBits::eVideoEncodeKHR) flag_names += "VideoEncodeKHR | ";
        if (flags & vk::QueueFlagBits::eOpticalFlowNV) flag_names += "OpticalFlowNV | ";
        if (flags & vk::QueueFlagBits::eDataGraphARM) flag_names += "DataGraphARM | ";

        auto surface_support = false;

        spdlog::info("queue_family_index: {}, queue_count: {}, surface_support: {}, queue_flags: {}", i,
                     queue_family_property.queueCount, surface_support,
                     flag_names);

        if (flags & vk::QueueFlagBits::eCompute && !(flags & vk::QueueFlagBits::eGraphics)) {
            queue_family_index = i;
        }

        ++i;
    }

    // vk::DeviceQueueCreateInfo device_queue_create_info { .queueFamilyIndex = graphicsIndex };

    // while (!glfwWindowShouldClose(window)) {
    //     glfwPollEvents();
    // }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
