#include <iostream>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>
#include <spdlog/spdlog.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <fstream>
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

std::vector<char> read_file(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", filename);
    }

    std::vector<char> buffer(file.tellg());

    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    file.close();

    return buffer;
}

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

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(const vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                       const vk::DebugUtilsMessageTypeFlagsEXT type,
                                                       const vk::DebugUtilsMessengerCallbackDataEXT *p_callback_data,
                                                       void *) {
    spdlog::log(to_spdlog_level(severity), "[Vulkan {}] {}", message_type_to_string(type), p_callback_data->pMessage);

    return vk::False;
}

std::string_view get_physical_device_type_name(const vk::PhysicalDeviceType type) {
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

vk::Extent2D choose_swapchain_extent(GLFWwindow *window, const vk::SurfaceCapabilitiesKHR &capabilities) {
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

vk::Format sRGB_to_UNorm(vk::Format format) {
    switch (format) {
        case vk::Format::eR8Srgb: return vk::Format::eR8Unorm;
        case vk::Format::eR8G8Srgb: return vk::Format::eR8G8Unorm;
        case vk::Format::eR8G8B8Srgb: return vk::Format::eR8G8B8Unorm;
        case vk::Format::eB8G8R8Srgb: return vk::Format::eB8G8R8Unorm;
        case vk::Format::eR8G8B8A8Srgb: return vk::Format::eR8G8B8A8Unorm;
        case vk::Format::eB8G8R8A8Srgb: return vk::Format::eB8G8R8A8Unorm;

        default: return format;
    }
}

vk::Buffer create_buffer(VmaAllocator allocator,
                         vk::DeviceSize size, vk::BufferUsageFlags usage,
                         VmaAllocation &allocation, VmaAllocationCreateFlags allocation_create_flags = 0) {
    const vk::BufferCreateInfo buffer_create_info = {
        .size = size,
        .usage = usage,
    };

    const VmaAllocationCreateInfo allocation_create_info = {
        .flags = allocation_create_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    VkBuffer buffer;
    const auto result = vmaCreateBuffer(allocator, buffer_create_info, &allocation_create_info, &buffer, &allocation,
                                        nullptr);

    if (result != VK_SUCCESS) {
        spdlog::error("vmaCreateBuffer failed: {}", vk::to_string(static_cast<vk::Result>(result)));
    }

    return buffer;
}

vk::Image create_image(vk::ImageCreateInfo image_create_info, VmaAllocator allocator, VmaAllocation &allocation,
                       VmaAllocationCreateFlags allocation_create_flags = 0) {
    const VmaAllocationCreateInfo allocation_create_info = {
        .flags = allocation_create_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VkImage image;
    const auto result = vmaCreateImage(allocator, image_create_info, &allocation_create_info, &image, &allocation,
                                       nullptr);

    if (result != VK_SUCCESS) {
        spdlog::error("vmaCreateImage failed: {}", vk::to_string(static_cast<vk::Result>(result)));
    }

    return image;
}

void layout_transition(vk::raii::CommandBuffer &cmd_buffer, vk::Image image,
                       vk::ImageLayout old_layout, vk::ImageLayout new_layout) {
    vk::AccessFlags2 srcAccessMask;
    vk::AccessFlags2 dstAccessMask;
    vk::PipelineStageFlags2 srcStageMask;
    vk::PipelineStageFlags2 dstStageMask;

    if (old_layout == vk::ImageLayout::eUndefined &&
        new_layout == vk::ImageLayout::eTransferDstOptimal) {
        srcAccessMask = vk::AccessFlagBits2::eNone;
        dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
        srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    } else if (old_layout == vk::ImageLayout::eUndefined &&
               new_layout == vk::ImageLayout::eTransferSrcOptimal) {
        srcAccessMask = vk::AccessFlagBits2::eNone;
        dstAccessMask = vk::AccessFlagBits2::eTransferRead;
        srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
               new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
    } else if (old_layout == vk::ImageLayout::eUndefined &&
               new_layout == vk::ImageLayout::eGeneral) {
        srcAccessMask = vk::AccessFlagBits2::eNone;
        dstAccessMask = vk::AccessFlagBits2::eShaderWrite;
        srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR;
    } else if (old_layout == vk::ImageLayout::eGeneral &&
               new_layout == vk::ImageLayout::eTransferSrcOptimal) {
        srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
        dstAccessMask = vk::AccessFlagBits2::eTransferRead;
        srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR;
        dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
               new_layout == vk::ImageLayout::ePresentSrcKHR) {
        srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        dstAccessMask = vk::AccessFlagBits2::eNone;
        srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;
    } else if (old_layout == vk::ImageLayout::eTransferSrcOptimal &&
               new_layout == vk::ImageLayout::eGeneral) {
        srcAccessMask = vk::AccessFlagBits2::eTransferRead;
        dstAccessMask = vk::AccessFlagBits2::eShaderWrite;
        srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR;
    } else {
        spdlog::error("Unsupported layout transition");
    }

    vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    vk::DependencyInfo dependency_info{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    cmd_buffer.pipelineBarrier2(dependency_info);
}

vk::raii::CommandBuffer begin_single_time_commands(const vk::raii::Device &device,
                                                   const vk::raii::CommandPool &cmd_pool) {
    vk::CommandBufferAllocateInfo alloc_info{
        .commandPool = *cmd_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };
    auto cmd_buffer = std::move(vk::raii::CommandBuffers(device, alloc_info).front());

    vk::CommandBufferBeginInfo begin_info{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    cmd_buffer.begin(begin_info);

    return cmd_buffer;
}

void end_single_time_commands(const vk::raii::Queue &queue, vk::raii::CommandBuffer &cmd_buffer) {
    cmd_buffer.end();

    vk::SubmitInfo submit_info{
        .commandBufferCount = 1,
        .pCommandBuffers = &*cmd_buffer,
    };
    queue.submit(submit_info, nullptr);
    queue.waitIdle();
}

uint32_t round_up(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

void init_vulkan(GLFWwindow *window) {
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
                "Required extension not supported: " + std::string(extension));
        }
    }

    constexpr vk::ApplicationInfo app_info{
        .pApplicationName = "HWRT",
        .apiVersion = vk::ApiVersion14,
    };

    std::vector validation_features_enabled_list{
        //// vk::ValidationFeatureEnableEXT::eDebugPrintf,
        vk::ValidationFeatureEnableEXT::eBestPractices,
        vk::ValidationFeatureEnableEXT::eSynchronizationValidation,
        // vk::ValidationFeatureEnableEXT::eGpuAssisted,
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
    required_device_extensions.push_back(vk::KHRDeferredHostOperationsExtensionName);
    required_device_extensions.push_back(vk::KHRAccelerationStructureExtensionName);
    required_device_extensions.push_back(vk::KHRRayTracingPipelineExtensionName);
    // required_device_extensions.push_back(vk::KHRPushDescriptorExtensionName);
    // required_device_extensions.push_back(vk::KHRDynamicRenderingExtensionName);

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

    // Surface

    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
        throw std::runtime_error("Failed to create window surface");
    }
    auto surface = vk::raii::SurfaceKHR(instance, _surface);

    // Device & Queue

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

        if (flags & vk::QueueFlagBits::eCompute && (flags & vk::QueueFlagBits::eGraphics)) {
            queue_family_index = i;
        }

        ++i;
    }
    spdlog::info("Selected queue family {}", queue_family_index);

    vk::PhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features{
        .bufferDeviceAddress = vk::True,
    };

    vk::PhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{
        .pNext = buffer_device_address_features,
        .accelerationStructure = vk::True,
    };

    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features{
        .pNext = acceleration_structure_features,
        .rayTracingPipeline = vk::True,
    };

    // auto features2 = adapter.getFeatures2();

    vk::PhysicalDeviceVulkan13Features vulkan13_features{
        .pNext = ray_tracing_pipeline_features,
        .synchronization2 = vk::True,
    };

    vk::PhysicalDeviceVulkan14Features vulkan14_features{
        .pNext = vulkan13_features,
        .pushDescriptor = vk::True,
    };

    // vulkan13_features.dynamicRendering = vk::True;

    // features2.pNext = &vulkan13_features;

    float queue_priority = 1.0f;
    vk::DeviceQueueCreateInfo device_queue_create_info{
        .queueFamilyIndex = static_cast<uint32_t>(queue_family_index),
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    vk::DeviceCreateInfo device_create_info{
        .pNext = &vulkan14_features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &device_queue_create_info,
        .enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size()),
        .ppEnabledExtensionNames = required_device_extensions.data(),
    };

    auto device = vk::raii::Device(adapter, device_create_info);
    auto queue = vk::raii::Queue(device, queue_family_index, 0);

    // Swapchain

    auto surface_capabilities = adapter.getSurfaceCapabilitiesKHR(*surface);
    auto swapchain_extent = choose_swapchain_extent(window, surface_capabilities);

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
        .imageUsage = vk::ImageUsageFlagBits::eTransferDst,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = vk::PresentModeKHR::eImmediate,
        .clipped = true
    };

    auto swapchain = vk::raii::SwapchainKHR(device, swapchain_create_info);
    std::vector<vk::Image> swapchain_images = swapchain.getImages();

    // vk::ImageViewCreateInfo image_view_create_info{
    //     .viewType = vk::ImageViewType::e2D,
    //     .format = swapchain_surface_format.format,
    //     .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
    // };
    // std::vector<vk::raii::ImageView> swapchain_image_views;
    // for (auto image: swapchain_images) {
    //     image_view_create_info.image = image;
    //     swapchain_image_views.emplace_back(device, image_view_create_info);
    // }

    VmaAllocatorCreateInfo allocator_create_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = static_cast<vk::PhysicalDevice>(adapter),
        .device = static_cast<vk::Device>(device),
        .instance = static_cast<vk::Instance>(instance),
    };

    VmaAllocator allocator;

    if (vmaCreateAllocator(&allocator_create_info, &allocator) != VK_SUCCESS) {
        spdlog::error("Failed to create allocator");
    }

    VmaAllocation vertex_allocation, index_allocation;

    vk::Buffer vertex_buffer = create_buffer(
        allocator,
        sizeof(Vertex) * vertices.size(),
        vk::BufferUsageFlagBits::eVertexBuffer
        | vk::BufferUsageFlagBits::eStorageBuffer
        | vk::BufferUsageFlagBits::eShaderDeviceAddress
        | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        vertex_allocation,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    {
        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(allocator, vertex_allocation, &allocation_info);
        memcpy(allocation_info.pMappedData, vertices.data(), sizeof(Vertex) * vertices.size());
    }

    vk::Buffer index_buffer = create_buffer(
        allocator,
        sizeof(uint32_t) * indices.size(),
        vk::BufferUsageFlagBits::eIndexBuffer
        | vk::BufferUsageFlagBits::eStorageBuffer
        | vk::BufferUsageFlagBits::eShaderDeviceAddress
        | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        index_allocation,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    {
        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(allocator, index_allocation, &allocation_info);
        memcpy(allocation_info.pMappedData, indices.data(), sizeof(u_int32_t) * indices.size());
    }

    vk::BufferDeviceAddressInfo vertex_buffer_address_info{
        .buffer = vertex_buffer,
    };
    vk::DeviceAddress vertex_address = device.getBufferAddress(vertex_buffer_address_info);

    vk::BufferDeviceAddressInfo index_buffer_address_info{
        .buffer = index_buffer,
    };
    vk::DeviceAddress index_address = device.getBufferAddress(index_buffer_address_info);

    // Create Bottom Level Acceleration Structure

    auto triangle_count = static_cast<uint32_t>(indices.size() / 3);

    vk::AccelerationStructureGeometryTrianglesDataKHR BLAS_geometry_triangles_data{
        .vertexFormat = vk::Format::eR32G32B32Sfloat,
        .vertexData = vertex_address,
        .vertexStride = sizeof(Vertex),
        .maxVertex = static_cast<uint32_t>(vertices.size() - 1),
        .indexType = vk::IndexType::eUint32,
        .indexData = index_address,
    };

    vk::AccelerationStructureGeometryKHR BLAS_geometry{
        .geometryType = vk::GeometryTypeKHR::eTriangles,
        .geometry = BLAS_geometry_triangles_data,
        .flags = vk::GeometryFlagBitsKHR::eOpaque,
    };

    vk::AccelerationStructureBuildRangeInfoKHR BLAS_build_range_info{
        .primitiveCount = triangle_count,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    };

    vk::AccelerationStructureBuildGeometryInfoKHR BLAS_build_geometry_info{
        .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries = &BLAS_geometry,
    };

    std::vector<uint32_t> BLAS_max_primitive_counts(1);
    BLAS_max_primitive_counts[0] = BLAS_build_range_info.primitiveCount;

    auto BLAS_build_sizes_info = device.getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice, BLAS_build_geometry_info,
        BLAS_max_primitive_counts);

    VmaAllocation BLAS_allocation;

    auto BLAS_buffer = create_buffer(
        allocator,
        BLAS_build_sizes_info.accelerationStructureSize,
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
        | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        BLAS_allocation,
        0
    );

    vk::AccelerationStructureCreateInfoKHR BLAS_create_info{
        .buffer = BLAS_buffer,
        .size = BLAS_build_sizes_info.accelerationStructureSize,
        .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
    };

    vk::CommandPoolCreateInfo cmd_pool_create_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = static_cast<uint32_t>(queue_family_index),
    };
    auto cmd_pool = vk::raii::CommandPool(device, cmd_pool_create_info);

    auto single_time_cmd_buffer = begin_single_time_commands(device, cmd_pool);

    constexpr int frames_in_flight = 2;

    auto BLAS = device.createAccelerationStructureKHR(BLAS_create_info);

    // Build Bottom Level Acceleration Structure

    VmaAllocation BLAS_scratch_allocation;

    auto BLAS_scratch_buffer = create_buffer(
        allocator,
        BLAS_build_sizes_info.buildScratchSize,
        vk::BufferUsageFlagBits::eStorageBuffer
        | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        BLAS_scratch_allocation,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
    );

    vk::BufferDeviceAddressInfo BLAS_scratch_buffer_device_address_info{
        .buffer = BLAS_scratch_buffer,
    };
    auto BLAS_scratch_buffer_device_address = device.getBufferAddress(BLAS_scratch_buffer_device_address_info);

    BLAS_build_geometry_info.dstAccelerationStructure = BLAS;
    BLAS_build_geometry_info.scratchData = BLAS_scratch_buffer_device_address;

    single_time_cmd_buffer.buildAccelerationStructuresKHR({BLAS_build_geometry_info}, {&BLAS_build_range_info});

    vk::MemoryBarrier2 AS_build_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        .srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
        .dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
        .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR,
    };

    vk::DependencyInfo dependency_info_as_build{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &AS_build_barrier,
    };

    single_time_cmd_buffer.pipelineBarrier2(dependency_info_as_build);

    // Create Top Level Acceleration Structure

    vk::AccelerationStructureDeviceAddressInfoKHR BLAS_device_address_info{
        .accelerationStructure = BLAS
    };
    auto BLAS_device_address = device.getAccelerationStructureAddressKHR(BLAS_device_address_info);

    vk::TransformMatrixKHR transform{
        std::array<std::array<float, 4>, 3>{
            {
                {{1.f, 0.f, 0.f, 0.f}},
                {{0.f, 1.f, 0.f, 0.f}},
                {{0.f, 0.f, 1.f, 0.f}},
            }
        }
    };

    vk::AccelerationStructureInstanceKHR BLAS_instance{
        .transform = transform,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = BLAS_device_address
    };

    VmaAllocation BLAS_instance_allocation;

    auto BLAS_instance_buffer = create_buffer(
        allocator,
        sizeof(vk::AccelerationStructureInstanceKHR),
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
        | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        BLAS_instance_allocation,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    {
        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(allocator, BLAS_instance_allocation, &allocation_info);
        memcpy(allocation_info.pMappedData, &BLAS_instance, sizeof(vk::AccelerationStructureInstanceKHR));
    }

    vk::BufferDeviceAddressInfo BLAS_instance_buffer_address_info{
        .buffer = BLAS_instance_buffer
    };
    auto BLAS_instance_device_address = device.getBufferAddress(BLAS_instance_buffer_address_info);

    vk::AccelerationStructureGeometryInstancesDataKHR TLAS_geometry_instances_data{
        .arrayOfPointers = vk::False,
        .data = BLAS_instance_device_address,
    };

    vk::AccelerationStructureGeometryKHR TLAS_geometry{
        .geometryType = vk::GeometryTypeKHR::eInstances,
        .geometry = TLAS_geometry_instances_data,
        .flags = vk::GeometryFlagBitsKHR::eOpaque,
    };

    vk::AccelerationStructureBuildRangeInfoKHR TLAS_build_range_info{
        .primitiveCount = 1,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    };

    vk::AccelerationStructureBuildGeometryInfoKHR TLAS_build_geometry_info{
        .type = vk::AccelerationStructureTypeKHR::eTopLevel,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries = &TLAS_geometry,
    };

    std::vector<uint32_t> TLAS_max_primitive_counts(1);
    TLAS_max_primitive_counts[0] = TLAS_build_range_info.primitiveCount;

    auto TLAS_build_sizes_info = device.getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice, TLAS_build_geometry_info,
        TLAS_max_primitive_counts);

    VmaAllocation TLAS_allocation;

    auto TLAS_buffer = create_buffer(
        allocator,
        TLAS_build_sizes_info.accelerationStructureSize,
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR/*
        | vk::BufferUsageFlagBits::eShaderDeviceAddress*/,
        TLAS_allocation,
        0
    );

    vk::AccelerationStructureCreateInfoKHR TLAS_create_info{
        .buffer = TLAS_buffer,
        .size = TLAS_build_sizes_info.accelerationStructureSize,
        .type = vk::AccelerationStructureTypeKHR::eTopLevel,
    };

    auto TLAS = device.createAccelerationStructureKHR(TLAS_create_info);

    // Build Top Level Acceleration Structure

    VmaAllocation TLAS_scratch_allocation;

    auto TLAS_scratch_buffer = create_buffer(
        allocator,
        TLAS_build_sizes_info.buildScratchSize,
        vk::BufferUsageFlagBits::eStorageBuffer
        | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        TLAS_scratch_allocation,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
    );

    vk::BufferDeviceAddressInfo TLAS_scratch_buffer_device_address_info{
        .buffer = TLAS_scratch_buffer,
    };
    auto TLAS_scratch_buffer_device_address = device.getBufferAddress(TLAS_scratch_buffer_device_address_info);

    TLAS_build_geometry_info.dstAccelerationStructure = TLAS;
    TLAS_build_geometry_info.scratchData = TLAS_scratch_buffer_device_address;

    single_time_cmd_buffer.buildAccelerationStructuresKHR({TLAS_build_geometry_info}, {&TLAS_build_range_info});

    // Ray Trace Image

    auto queue_family_index_u32 = static_cast<uint32_t>(queue_family_index);

    vk::ImageCreateInfo ray_trace_image_create_info{
        .imageType = vk::ImageType::e2D,
        .format = sRGB_to_UNorm(swapchain_surface_format.format),
        .extent = vk::Extent3D{
            swapchain_extent.width,
            swapchain_extent.height,
            1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queue_family_index_u32,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    VmaAllocation ray_trace_image_allocation;

    auto ray_trace_image = create_image(ray_trace_image_create_info, allocator, ray_trace_image_allocation);

    vk::ImageViewCreateInfo ray_trace_image_view_create_info{
        .image = ray_trace_image,
        .viewType = vk::ImageViewType::e2D,
        .format = sRGB_to_UNorm(swapchain_surface_format.format),
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    };
    auto ray_trace_image_view = device.createImageView(ray_trace_image_view_create_info);

    layout_transition(single_time_cmd_buffer, ray_trace_image, vk::ImageLayout::eUndefined,
                      vk::ImageLayout::eGeneral);

    end_single_time_commands(queue, single_time_cmd_buffer);

    // Push Descriptors & Pipeline Layout

    std::vector<vk::DescriptorSetLayoutBinding> bindings;

    bindings.push_back(
        vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eAll,
        }
    );

    bindings.push_back(
        vk::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vk::DescriptorType::eStorageImage,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eAll,
        }
    );

    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptor,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    auto descriptor_set_layout = device.createDescriptorSetLayout(descriptor_set_layout_create_info);

    vk::WriteDescriptorSetAccelerationStructureKHR write_AS_info{
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &*TLAS,
    };

    vk::WriteDescriptorSet write_AS{
        .pNext = &write_AS_info,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
    };

    vk::DescriptorImageInfo descriptor_image_info{
        .imageView = ray_trace_image_view,
        .imageLayout = vk::ImageLayout::eGeneral,
    };

    vk::WriteDescriptorSet write_image{
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eStorageImage,
        .pImageInfo = &descriptor_image_info,
    };

    std::vector writes{write_AS, write_image};

    vk::PipelineLayoutCreateInfo pipeline_layout_create_info{
        .setLayoutCount = 1,
        .pSetLayouts = &*descriptor_set_layout,
    };

    auto pipeline_layout = device.createPipelineLayout(pipeline_layout_create_info);

    // Ray Tracing Pipeline

    auto ray_gen_shader_code = read_file("../src/shaders/spirv/raytrace.rgen.spv");
    vk::ShaderModuleCreateInfo ray_gen_shader_module_create_info{
        .codeSize = ray_gen_shader_code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t *>(ray_gen_shader_code.data()),
    };
    auto ray_gen_shader_module = device.createShaderModule(ray_gen_shader_module_create_info);

    auto ray_miss_shader_code = read_file("../src/shaders/spirv/raytrace.rmiss.spv");
    vk::ShaderModuleCreateInfo ray_miss_shader_module_create_info{
        .codeSize = ray_miss_shader_code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t *>(ray_miss_shader_code.data()),
    };
    auto ray_miss_shader_module = device.createShaderModule(ray_miss_shader_module_create_info);

    auto closest_hit_shader_code = read_file("../src/shaders/spirv/raytrace.rchit.spv");
    vk::ShaderModuleCreateInfo closest_hit_shader_module_create_info{
        .codeSize = closest_hit_shader_code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t *>(closest_hit_shader_code.data()),
    };
    auto closest_hit_shader_module = device.createShaderModule(closest_hit_shader_module_create_info);

    std::vector shader_stage_create_info_list = {
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eRaygenKHR,
            .module = ray_gen_shader_module,
            .pName = "main",
        },
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eMissKHR,
            .module = ray_miss_shader_module,
            .pName = "main",
        },
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eClosestHitKHR,
            .module = closest_hit_shader_module,
            .pName = "main",
        },
    };

    std::vector ray_tracing_shader_group_create_info_list = {
        vk::RayTracingShaderGroupCreateInfoKHR{
            .type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
            .generalShader = 0,
        },
        vk::RayTracingShaderGroupCreateInfoKHR{
            .type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
            .generalShader = 1,
        },
        vk::RayTracingShaderGroupCreateInfoKHR{
            .type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
            .closestHitShader = 2,
        },
    };

    vk::RayTracingPipelineCreateInfoKHR ray_tracing_pipeline_create_info{
        .stageCount = static_cast<uint32_t>(shader_stage_create_info_list.size()),
        .pStages = shader_stage_create_info_list.data(),
        .groupCount = static_cast<uint32_t>(ray_tracing_shader_group_create_info_list.size()),
        .pGroups = ray_tracing_shader_group_create_info_list.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = pipeline_layout,
    };

    auto ray_tracing_pipeline = device.createRayTracingPipelineKHR(nullptr, nullptr, ray_tracing_pipeline_create_info);

    // Shader Binding Table

    vk::StructureChain<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>
            ray_tracing_pipeline_properties_chain =
                    adapter.getProperties2<vk::PhysicalDeviceProperties2,
                        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    auto ray_tracing_pipeline_properties = ray_tracing_pipeline_properties_chain.get<
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    uint32_t handle_size = ray_tracing_pipeline_properties.shaderGroupHandleSize;
    uint32_t handle_alignment = ray_tracing_pipeline_properties.shaderGroupHandleAlignment;
    uint32_t base_alignment = ray_tracing_pipeline_properties.shaderGroupBaseAlignment;
    uint32_t group_count = ray_tracing_pipeline_create_info.groupCount;

    size_t data_size = handle_size * group_count;

    std::vector<uint8_t> shader_handles;
    shader_handles.reserve(data_size);

    PFN_vkGetRayTracingShaderGroupHandlesKHR pfn_vkGetRayTracingShaderGroupHandlesKHR;

    pfn_vkGetRayTracingShaderGroupHandlesKHR =
            reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(
                *device,
                "vkGetRayTracingShaderGroupHandlesKHR"
            ));

    (void) pfn_vkGetRayTracingShaderGroupHandlesKHR(*device, *ray_tracing_pipeline, 0, group_count, data_size,
                                                    shader_handles.data());

    auto align_up = [](uint32_t size, uint32_t alignment) { return (size + alignment - 1) & ~(alignment - 1); };
    uint32_t rgen_size = align_up(handle_size, handle_alignment);
    uint32_t rmiss_size = align_up(handle_size, handle_alignment);
    uint32_t rchit_size = align_up(handle_size, handle_alignment);
    uint32_t callable_size = 0;

    uint32_t rgen_offset = 0;
    uint32_t rmiss_offset = align_up(rgen_size, base_alignment);
    uint32_t rchit_offset = align_up(rmiss_offset + rmiss_size, base_alignment);
    uint32_t callable_offset = align_up(rchit_offset + rchit_size, base_alignment);

    size_t buffer_size = callable_offset + callable_size;

    vk::StridedDeviceAddressRegionKHR rgen_region{};
    vk::StridedDeviceAddressRegionKHR rmiss_region{};
    vk::StridedDeviceAddressRegionKHR rchit_region{};
    vk::StridedDeviceAddressRegionKHR callable_region{};

    VmaAllocation shader_binding_table_allocation;

    vk::Buffer shader_binding_table_buffer = create_buffer(
        allocator,
        buffer_size,
        vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eShaderBindingTableKHR,
        shader_binding_table_allocation,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
        | VMA_ALLOCATION_CREATE_MAPPED_BIT
    );

    {
        vk::BufferDeviceAddressInfo shader_binding_table_buffer_address_info{
            .buffer = shader_binding_table_buffer,
        };
        vk::DeviceAddress shader_binding_table_address = device.getBufferAddress(
            shader_binding_table_buffer_address_info);

        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(allocator, shader_binding_table_allocation, &allocation_info);

        auto *p_data = static_cast<uint8_t *>(allocation_info.pMappedData);

        memcpy(p_data + rgen_offset, shader_handles.data() + 0 * handle_size, handle_size);
        rgen_region.deviceAddress = shader_binding_table_address + rgen_offset;
        rgen_region.stride = rgen_size;
        rgen_region.size = rgen_size;

        memcpy(p_data + rmiss_offset, shader_handles.data() + 1 * handle_size, handle_size);
        rmiss_region.deviceAddress = shader_binding_table_address + rmiss_offset;
        rmiss_region.stride = rmiss_size;
        rmiss_region.size = rmiss_size;

        memcpy(p_data + rchit_offset, shader_handles.data() + 2 * handle_size, handle_size);
        rchit_region.deviceAddress = shader_binding_table_address + rchit_offset;
        rchit_region.stride = rchit_size;
        rchit_region.size = rchit_size;

        callable_region.deviceAddress = 0;
        callable_region.stride = 0;
        callable_region.size = 0;
    }

    std::vector<vk::raii::Semaphore> image_available_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;
    std::vector<vk::raii::Fence> render_fences;

    vk::CommandBufferAllocateInfo cmd_buffer_allocate_info{
        .commandPool = cmd_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = frames_in_flight,
    };
    auto cmd_buffers = vk::raii::CommandBuffers(device, cmd_buffer_allocate_info);

    for (size_t i = 0; i < frames_in_flight; ++i) {
        vk::FenceCreateInfo fence_create_info{
            .flags = vk::FenceCreateFlagBits::eSignaled
        };
        render_fences.emplace_back(device, fence_create_info);

        vk::SemaphoreCreateInfo semaphore_info;
        image_available_semaphores.emplace_back(device, semaphore_info);
    }

    for (size_t i = 0; i < swapchain_images.size(); ++i) {
        vk::SemaphoreCreateInfo semaphore_info;
        render_finished_semaphores.emplace_back(device, semaphore_info);
    }

    uint32_t frame_index = 0;

    queue.waitIdle();

    while (!glfwWindowShouldClose(window)) {
        (void) device.waitForFences({*render_fences[frame_index]}, vk::True, std::numeric_limits<uint64_t>::max());
        device.resetFences({*render_fences[frame_index]});

        uint32_t image_index;

        auto &current_image_available_semaphore = image_available_semaphores[frame_index];

        vk::AcquireNextImageInfoKHR acquire_info{
            .swapchain = swapchain,
            .timeout = std::numeric_limits<uint64_t>::max(),
            .semaphore = current_image_available_semaphore,
            .deviceMask = 1,
        };
        image_index = device.acquireNextImage2KHR(acquire_info).value;
        auto current_swapchain_image = swapchain_images[image_index];

        auto &current_render_finished_semaphore = render_finished_semaphores[image_index];

        auto &cmd_buffer = cmd_buffers[frame_index];

        cmd_buffer.begin({});

        cmd_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, ray_tracing_pipeline);
        cmd_buffer.pushDescriptorSet(vk::PipelineBindPoint::eRayTracingKHR, pipeline_layout, 0, writes);
        cmd_buffer.traceRaysKHR(rgen_region, rmiss_region, rchit_region, callable_region,
                                swapchain_extent.width,
                                swapchain_extent.height, 1);

        layout_transition(cmd_buffer, ray_trace_image, vk::ImageLayout::eGeneral,
                          vk::ImageLayout::eTransferSrcOptimal);
        layout_transition(cmd_buffer, current_swapchain_image, vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal);

        vk::ImageCopy copy_region{
            .srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            .dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            .extent = vk::Extent3D{swapchain_extent.width, swapchain_extent.height, 1},
        };
        cmd_buffer.copyImage(
            ray_trace_image, vk::ImageLayout::eTransferSrcOptimal,
            current_swapchain_image, vk::ImageLayout::eTransferDstOptimal,
            copy_region
        );

        layout_transition(cmd_buffer, current_swapchain_image, vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::ePresentSrcKHR);
        layout_transition(cmd_buffer, ray_trace_image, vk::ImageLayout::eTransferSrcOptimal,
                          vk::ImageLayout::eGeneral);

        cmd_buffer.end();

        vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eTopOfPipe;

        vk::SubmitInfo render_submit_info{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*current_image_available_semaphore,
            .pWaitDstStageMask = &wait_stage,

            .commandBufferCount = 1,
            .pCommandBuffers = &*cmd_buffer,

            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &*current_render_finished_semaphore,
        };

        queue.submit(render_submit_info, *render_fences[frame_index]);

        vk::PresentInfoKHR present_info{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*current_render_finished_semaphore,
            .swapchainCount = 1,
            .pSwapchains = &*swapchain,
            .pImageIndices = &image_index,
        };

        (void) queue.presentKHR(present_info);

        static auto last_update_time = std::chrono::high_resolution_clock::now();
        static int frame_count = 0;

        frame_count++;

        auto current_time = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> elapsed_time = current_time - last_update_time;

        if (elapsed_time.count() >= 1.0) {
            double fps = static_cast<double>(frame_count) / elapsed_time.count();

            spdlog::info("{:.0f} fps ({:.2f} ms)",
                         fps, 1000.0 / fps);

            frame_count = 0;
            last_update_time = current_time;
        }

        glfwPollEvents();

        frame_index = (frame_index + 1) % frames_in_flight;
    }

    device.waitIdle();

    vmaDestroyImage(allocator, ray_trace_image, ray_trace_image_allocation);
    vmaDestroyBuffer(allocator, shader_binding_table_buffer, shader_binding_table_allocation);
    vmaDestroyBuffer(allocator, TLAS_scratch_buffer, TLAS_scratch_allocation);
    vmaDestroyBuffer(allocator, TLAS_buffer, TLAS_allocation);
    vmaDestroyBuffer(allocator, BLAS_instance_buffer, BLAS_instance_allocation);
    vmaDestroyBuffer(allocator, BLAS_scratch_buffer, BLAS_scratch_allocation);
    vmaDestroyBuffer(allocator, BLAS_buffer, BLAS_allocation);
    vmaDestroyBuffer(allocator, vertex_buffer, vertex_allocation);
    vmaDestroyBuffer(allocator, index_buffer, index_allocation);
    vmaDestroyAllocator(allocator);
}

int main() {
#ifdef NDEBUG
    spdlog::set_level(spdlog::level::info);
#else
    spdlog::set_level(spdlog::level::trace);
#endif

    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *window = glfwCreateWindow(800, 600, "HWRT", nullptr, nullptr);

    if (!window) {
        spdlog::error("Failed to create GLFW window");
        glfwTerminate();
        return 1;
    }

    {
        // Need for cleanup Vulkan RAII objects before GLFW
        init_vulkan(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
