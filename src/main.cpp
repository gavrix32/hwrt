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

void layout_transition(vk::raii::CommandBuffer &cmd, vk::Image image,
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
        dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
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

    cmd.pipelineBarrier2(dependency_info);
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
    required_device_extensions.push_back(vk::KHRDeferredHostOperationsExtensionName);
    required_device_extensions.push_back(vk::KHRAccelerationStructureExtensionName);
    // required_device_extensions.push_back(vk::KHRDynamicRenderingExtensionName);
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

        if (flags & vk::QueueFlagBits::eCompute && !(flags & vk::QueueFlagBits::eGraphics)) {
            queue_family_index = i;
        }

        ++i;
    }
    spdlog::info("Selected queue family {}", queue_family_index);

    vk::PhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features{
        .bufferDeviceAddress = vk::True
    };

    vk::PhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{
        .pNext = buffer_device_address_features,
        .accelerationStructure = vk::True
    };

    // vk::PhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features_khr{
    // .rayTracingPipeline = vk::True
    // };

    // auto features2 = adapter.getFeatures2();

    vk::PhysicalDeviceVulkan13Features vulkan13_features{
        .pNext = acceleration_structure_features,
        .synchronization2 = vk::True,
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
        .pNext = &vulkan13_features,
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
        vmaGetAllocationInfo(allocator, vertex_allocation, &allocation_info);
        memcpy(allocation_info.pMappedData, vertices.data(), sizeof(Vertex) * vertices.size());
    }

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

    // Acceleration Structure

    auto triangle_count = static_cast<uint32_t>(indices.size() / 3);

    vk::AccelerationStructureGeometryTrianglesDataKHR AS_geometry_triangles_data{
        .vertexFormat = vk::Format::eR32G32B32Sfloat,
        .vertexData = vertex_address,
        .vertexStride = sizeof(Vertex),
        .maxVertex = static_cast<uint32_t>(vertices.size() - 1),
        .indexType = vk::IndexType::eUint32,
        .indexData = index_address,
    };

    vk::AccelerationStructureGeometryKHR AS_geometry{
        .geometryType = vk::GeometryTypeKHR::eTriangles,
        .geometry = AS_geometry_triangles_data,
        .flags = vk::GeometryFlagBitsKHR::eOpaque,
    };

    vk::AccelerationStructureBuildRangeInfoKHR AS_build_range_info{
        .primitiveCount = triangle_count,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    };

    auto AS_type = vk::AccelerationStructureTypeKHR::eBottomLevel;

    vk::AccelerationStructureBuildGeometryInfoKHR AS_build_geometry_info{
        .type = AS_type,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries = &AS_geometry,
    };

    std::vector<uint32_t> max_primitive_counts(1);
    max_primitive_counts[0] = AS_build_range_info.primitiveCount;

    auto AS_build_sizes_info = device.getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice, AS_build_geometry_info,
        max_primitive_counts);

    VmaAllocation scratch_allocation;

    auto scratch_buffer = create_buffer(
        allocator,
        AS_build_sizes_info.buildScratchSize,
        vk::BufferUsageFlagBits::eStorageBuffer
        | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        scratch_allocation,
        0
    );

    vk::BufferDeviceAddressInfo scratch_buffer_address_info{
        .buffer = scratch_buffer,
    };
    auto scratch_address = device.getBufferAddress(scratch_buffer_address_info);

    VmaAllocation AS_allocation;

    auto AS_buffer = create_buffer(
        allocator,
        AS_build_sizes_info.accelerationStructureSize,
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
        | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        AS_allocation,
        0
    );

    vk::AccelerationStructureCreateInfoKHR AS_create_info{
        .buffer = AS_buffer,
        .size = AS_build_sizes_info.accelerationStructureSize,
        .type = AS_type,
    };

    vk::CommandPoolCreateInfo command_pool_create_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = static_cast<uint32_t>(queue_family_index),
    };
    auto command_pool = vk::raii::CommandPool(device, command_pool_create_info);

    vk::CommandBufferAllocateInfo command_buffer_allocate_info{
        .commandPool = command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };
    auto command_buffers = vk::raii::CommandBuffers(device, command_buffer_allocate_info);
    auto command_buffer = std::move(command_buffers.front());

    vk::CommandBufferBeginInfo begin_info{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    command_buffer.begin(begin_info);

    auto AS = device.createAccelerationStructureKHR(AS_create_info);

    AS_build_geometry_info.dstAccelerationStructure = AS;
    AS_build_geometry_info.scratchData = scratch_address;

    command_buffer.buildAccelerationStructuresKHR({AS_build_geometry_info}, {&AS_build_range_info});

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

    layout_transition(command_buffer, ray_trace_image, vk::ImageLayout::eUndefined,
                      vk::ImageLayout::eTransferSrcOptimal);

    // Push Descriptors

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
        .pAccelerationStructures = &*AS,
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

    std::vector<vk::WriteDescriptorSet> writes{write_AS, write_image};

    // command_buffer.pushDescriptorSet(vk::PipelineBindPoint::eRayTracingKHR, /* TODO: layout */, 0, writes);

    // Ray Tracing Pipeline

    command_buffer.end();

    vmaDestroyImage(allocator, ray_trace_image, ray_trace_image_allocation);
    vmaDestroyBuffer(allocator, AS_buffer, AS_allocation);
    vmaDestroyBuffer(allocator, scratch_buffer, scratch_allocation);
    vmaDestroyBuffer(allocator, vertex_buffer, vertex_allocation);
    vmaDestroyBuffer(allocator, index_buffer, index_allocation);
    vmaDestroyAllocator(allocator);
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
        // Need for cleanup Vulkan RAII objects before GLFW
        init_vulkan(window);
    }

    // while (!glfwWindowShouldClose(window)) {
    //     glfwPollEvents();
    // }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
