#include <iostream>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>
#include <spdlog/spdlog.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

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

struct Vertex {
    float pos[3];
    float color[3];
};

std::vector<Vertex> vertices = {
    {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
};

std::vector<uint32_t> indices = {
    0, 1, 2,
};

vk::Extent2D choos_swapchain_extent(GLFWwindow* window, const vk::SurfaceCapabilitiesKHR& capabilities) {
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

vk::Buffer create_buffer(VmaAllocator allocator,
                         vk::DeviceSize size, vk::BufferUsageFlags usage,
                         VmaAllocation &allocation, VmaAllocationCreateFlags allocation_create_flags = 0) {
    vk::BufferCreateInfo buffer_create_info = {
        .size = size,
        .usage = usage,
    };

    VmaAllocationCreateInfo allocation_create_info = {
        .flags = allocation_create_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VkBuffer buffer;
    auto result = vmaCreateBuffer(allocator, buffer_create_info, &allocation_create_info, &buffer, &allocation,
                                  nullptr);

    if (result != VK_SUCCESS) {
        spdlog::error("vmaCreateBuffer failed");
    }

    return buffer;
}

void init_vulkan(GLFWwindow* window) {
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
    const char **required_instance_extensions_data = glfwGetRequiredInstanceExtensions(&required_extension_count);

    std::vector required_instance_extensions(required_instance_extensions_data,
                                             required_instance_extensions_data + required_extension_count);

    if (enable_validation_layers) {
        required_instance_extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    auto available_extensions = context.enumerateInstanceExtensionProperties();

    for (auto &extension: required_instance_extensions) {
        bool found = false;
        for (auto &available_extension: available_extensions) {
            if (std::strcmp(available_extension.extensionName, extension) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(
                "Required GLFW extension not supported: " + std::string(extension));
        }
    }

    constexpr vk::ApplicationInfo app_info{
        .pApplicationName = "HWRT",
        .apiVersion = vk::ApiVersion14,
    };

    vk::InstanceCreateInfo instance_create_info{
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = static_cast<uint32_t>(required_instance_extensions.size()),
        .ppEnabledExtensionNames = required_instance_extensions.data(),
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

    std::vector<const char *> required_device_extensions;
    required_device_extensions.push_back(vk::KHRSwapchainExtensionName);
    // required_device_extensions.push_back(vk::KHRDynamicRenderingExtensionName);
    // required_device_extensions.push_back(vk::KHRDeferredHostOperationsExtensionName);
    // required_device_extensions.push_back(vk::KHRAccelerationStructureExtensionName);
    // required_device_extensions.push_back(vk::KHRRayTracingPipelineExtensionName);

    vk::raii::PhysicalDevice adapter = nullptr;

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
        for (const auto &extension_name: required_device_extensions) {
            if (!available_device_extension_names.contains(extension_name)) {
                missing_device_extension_names.insert(extension_name);
            }
        }

        if (missing_device_extension_names.empty()) {
            spdlog::info("    All required extensions supported. Using this device");
            adapter = physical_device;
            break;
        } else {
            spdlog::error("    Required device extensions missing: ");
            for (const auto &extension_name: missing_device_extension_names) {
                spdlog::error("        - {}", extension_name);
            }
        }

        ++i;
    }
    if (adapter == nullptr) {
        spdlog::critical("Failed to find a physical device with support for all required extensions");
    }
    // ................

    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
        throw std::runtime_error("Failed to create window surface");
    }
    auto surface = vk::raii::SurfaceKHR(instance, _surface);

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

        auto present_support = adapter.getSurfaceSupportKHR(i, surface);

        spdlog::debug("queue_family_index: {}, queue_count: {}, present_support: {}, queue_flags: {}", i,
                      queue_family_property.queueCount, present_support,
                      flag_names);

        if (flags & vk::QueueFlagBits::eCompute && !(flags & vk::QueueFlagBits::eGraphics)) {
            queue_family_index = i;
        }

        ++i;
    }
    spdlog::info("Selected queue family {}", queue_family_index);

    // vk::PhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features_khr{
    // .rayTracingPipeline = vk::True
    // };

    // auto features2 = adapter.getFeatures2();

    // vk::PhysicalDeviceVulkan13Features vulkan13_features;
    // vulkan13_features.dynamicRendering = vk::True;

    // features2.pNext = &vulkan13_features;

    float queue_priority = 1.0f;
    vk::DeviceQueueCreateInfo device_queue_create_info{
        .queueFamilyIndex = static_cast<uint32_t>(queue_family_index),
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    vk::DeviceCreateInfo device_create_info{
        // .pNext = &ray_tracing_pipeline_features_khr,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &device_queue_create_info,
        .enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size()),
        .ppEnabledExtensionNames = required_device_extensions.data(),
    };

    auto device = vk::raii::Device(adapter, device_create_info);
    auto queue = vk::raii::Queue(device, queue_family_index, 0);

    auto surface_capabilities = adapter.getSurfaceCapabilitiesKHR(*surface);
    auto swapchain_extent = choos_swapchain_extent(window, surface_capabilities);

    vk::SurfaceFormatKHR swapchain_surface_format;
    auto available_formats = adapter.getSurfaceFormatsKHR(*surface);
    for (const auto &available_format: available_formats) {
        if (available_format.format == vk::Format::eB8G8R8A8Srgb && available_format.colorSpace ==
            vk::ColorSpaceKHR::eSrgbNonlinear) {
            swapchain_surface_format = available_format;
            break;
        }
        swapchain_surface_format = available_formats[0];
    }

    vk::SwapchainCreateInfoKHR swapchain_create_info{
        .surface = *surface,
        .minImageCount = surface_capabilities.minImageCount,
        .imageFormat = swapchain_surface_format.format,
        .imageColorSpace = swapchain_surface_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = vk::PresentModeKHR::eImmediate,
        .clipped = true
    };

    auto swapchain = vk::raii::SwapchainKHR(device, swapchain_create_info);
    std::vector<vk::Image> swapchain_images = swapchain.getImages();

    vk::ImageViewCreateInfo image_view_create_info{
        .viewType = vk::ImageViewType::e2D,
        .format = swapchain_surface_format.format,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
    };
    std::vector<vk::raii::ImageView> swapchain_image_views;
    for (auto image: swapchain_images) {
        image_view_create_info.image = image;
        swapchain_image_views.emplace_back(device, image_view_create_info);
    }

    // VmaAllocator allocator_create_nfo = {
    //     .instance = instance,
    //     .physicalDevice = adapter,
    //     .device = device,
    // };
    //
    // VmaAllocation vertexAllocation, indexAllocation;

    // vk::Buffer vertex_buffer = create_buffer(
    //     allocator_create_nfo,
    //     sizeof(Vertex) * vertices.size(),
    //     vk::BufferUsageFlagBits:eVertex
    //     | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    //     | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    //     vertexAllocation,
    //     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    //       | VMA_ALLOCATION_CREATE_MAPPED_BIT
    // );
    //
    // VkBuffer indexBuffer = create_buffer(
    //     allocator_create_nfo,
    //     sizeof(uint32_t) * indices.size(),
    //     VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    //     | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    //     | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    //     indexAllocation,
    //     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    //       | VMA_ALLOCATION_CREATE_MAPPED_BIT
    // );


    // vk::AccelerationStructureGeometryTrianglesDataKHR geometry_data{
    // .vertexFormat = vk::Format::eR32G32B32Sfloat,
    // .vertexData
    // };
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

    {
        init_vulkan(window);
    }

    // while (!glfwWindowShouldClose(window)) {
    //     glfwPollEvents();
    // }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
