#include <iostream>
#include <fstream>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include "vulkan/instance.h"
#include "vulkan/adapter.h"
#include "vulkan/device.h"

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

std::vector<char> read_file(const std::string& filename) {
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

vk::Extent2D choose_swapchain_extent(GLFWwindow* window, const vk::SurfaceCapabilitiesKHR& capabilities) {
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

vk::Format sRGB_to_UNorm(const vk::Format format) {
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

vk::Buffer create_buffer(VmaAllocator allocator, vk::DeviceSize size, vk::BufferUsageFlags usage, VmaAllocation& allocation,
                         VmaAllocationCreateFlags allocation_create_flags = 0) {
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
    const auto result = vmaCreateBuffer(allocator, buffer_create_info, &allocation_create_info, &buffer, &allocation, nullptr);

    if (result != VK_SUCCESS) {
        spdlog::error("vmaCreateBuffer failed: {}", vk::to_string(static_cast<vk::Result>(result)));
    }
    return buffer;
}

vk::Image create_image(vk::ImageCreateInfo image_create_info, VmaAllocator allocator, VmaAllocation& allocation,
                       VmaAllocationCreateFlags allocation_create_flags = 0) {
    const VmaAllocationCreateInfo allocation_create_info = {
        .flags = allocation_create_flags,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VkImage image;
    const auto result = vmaCreateImage(allocator, image_create_info, &allocation_create_info, &image, &allocation, nullptr);

    if (result != VK_SUCCESS) {
        spdlog::error("vmaCreateImage failed: {}", vk::to_string(static_cast<vk::Result>(result)));
    }
    return image;
}

void layout_transition(const vk::raii::CommandBuffer& cmd_buffer, vk::Image image, vk::ImageLayout old_layout,
                       vk::ImageLayout new_layout) {
    vk::AccessFlags2 srcAccessMask;
    vk::AccessFlags2 dstAccessMask;
    vk::PipelineStageFlags2 srcStageMask;
    vk::PipelineStageFlags2 dstStageMask;

    if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
        srcAccessMask = vk::AccessFlagBits2::eNone;
        dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
        srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    } else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferSrcOptimal) {
        srcAccessMask = vk::AccessFlagBits2::eNone;
        dstAccessMask = vk::AccessFlagBits2::eTransferRead;
        srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
    } else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eGeneral) {
        srcAccessMask = vk::AccessFlagBits2::eNone;
        dstAccessMask = vk::AccessFlagBits2::eShaderWrite;
        srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
        dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR;
    } else if (old_layout == vk::ImageLayout::eGeneral && new_layout == vk::ImageLayout::eTransferSrcOptimal) {
        srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
        dstAccessMask = vk::AccessFlagBits2::eTransferRead;
        srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR;
        dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::ePresentSrcKHR) {
        srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        dstAccessMask = vk::AccessFlagBits2::eNone;
        srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;
    } else if (old_layout == vk::ImageLayout::eTransferSrcOptimal && new_layout == vk::ImageLayout::eGeneral) {
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

    const vk::DependencyInfo dependency_info{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    cmd_buffer.pipelineBarrier2(dependency_info);
}

vk::raii::CommandBuffer begin_single_time_commands(const vk::raii::Device& device, const vk::raii::CommandPool& cmd_pool) {
    const vk::CommandBufferAllocateInfo alloc_info{
        .commandPool = *cmd_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };
    auto cmd_buffer = std::move(vk::raii::CommandBuffers(device, alloc_info).front());

    constexpr vk::CommandBufferBeginInfo begin_info{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    cmd_buffer.begin(begin_info);

    return cmd_buffer;
}

void end_single_time_commands(const vk::raii::Queue& queue, const vk::raii::CommandBuffer& cmd_buffer) {
    cmd_buffer.end();

    const vk::SubmitInfo submit_info{
        .commandBufferCount = 1,
        .pCommandBuffers = &*cmd_buffer,
    };
    queue.submit(submit_info, nullptr);
    queue.waitIdle();
}

uint32_t round_up(const uint32_t value, const uint32_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

void init_vulkan(GLFWwindow* window) {
    auto instance = Instance(enable_validation_layers);

    std::vector<const char*> required_device_extensions;
    required_device_extensions.push_back(vk::KHRSwapchainExtensionName);
    required_device_extensions.push_back(vk::KHRDeferredHostOperationsExtensionName);
    required_device_extensions.push_back(vk::KHRAccelerationStructureExtensionName);
    required_device_extensions.push_back(vk::KHRRayTracingPipelineExtensionName);

    auto adapter = Adapter(instance, required_device_extensions);

    // Surface
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance.get(), window, nullptr, &_surface) != 0) {
        throw std::runtime_error("Failed to create window surface");
    }
    auto surface = vk::raii::SurfaceKHR(instance.get(), _surface);

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceVulkan14Features,
                       vk::PhysicalDeviceBufferDeviceAddressFeatures, vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
                       vk::PhysicalDeviceRayTracingPipelineFeaturesKHR> features_chain;

    features_chain.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = vk::True;
    features_chain.get<vk::PhysicalDeviceVulkan14Features>().pushDescriptor = vk::True;
    features_chain.get<vk::PhysicalDeviceBufferDeviceAddressFeatures>().bufferDeviceAddress = vk::True;
    features_chain.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>().accelerationStructure = vk::True;
    features_chain.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>().rayTracingPipeline = vk::True;

    auto device = Device(adapter, required_device_extensions, features_chain.get<vk::PhysicalDeviceFeatures2>());

    // Swapchain
    auto surface_capabilities = adapter.get().getSurfaceCapabilitiesKHR(*surface);
    auto swapchain_extent = choose_swapchain_extent(window, surface_capabilities);

    vk::SurfaceFormatKHR swapchain_surface_format;
    auto available_formats = adapter.get().getSurfaceFormatsKHR(*surface);
    for (const auto& available_format : available_formats) {
        if (available_format.format == vk::Format::eB8G8R8A8Srgb &&
            available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
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

    auto swapchain = vk::raii::SwapchainKHR(device.get(), swapchain_create_info);
    std::vector<vk::Image> swapchain_images = swapchain.getImages();

    VmaAllocatorCreateInfo allocator_create_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = static_cast<vk::PhysicalDevice>(adapter.get()),
        .device = static_cast<vk::Device>(device.get()),
        .instance = static_cast<vk::Instance>(instance.get()),
    };

    VmaAllocator allocator;
    if (vmaCreateAllocator(&allocator_create_info, &allocator) != VK_SUCCESS) {
        spdlog::error("Failed to create allocator");
    }

    VmaAllocation vertex_allocation, index_allocation;

    vk::Buffer vertex_buffer = create_buffer(allocator,
                                             sizeof(Vertex) * vertices.size(),
                                             vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                                             vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                             vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                                             vertex_allocation,
                                             VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                             VMA_ALLOCATION_CREATE_MAPPED_BIT);

    {
        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(allocator, vertex_allocation, &allocation_info);
        memcpy(allocation_info.pMappedData, vertices.data(), sizeof(Vertex) * vertices.size());
    }

    vk::Buffer index_buffer = create_buffer(allocator,
                                            sizeof(uint32_t) * indices.size(),
                                            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                                            vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                            vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                                            index_allocation,
                                            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                            VMA_ALLOCATION_CREATE_MAPPED_BIT);

    {
        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(allocator, index_allocation, &allocation_info);
        memcpy(allocation_info.pMappedData, indices.data(), sizeof(u_int32_t) * indices.size());
    }

    vk::BufferDeviceAddressInfo vertex_buffer_address_info{
        .buffer = vertex_buffer,
    };
    vk::DeviceAddress vertex_address = device.get().getBufferAddress(vertex_buffer_address_info);

    vk::BufferDeviceAddressInfo index_buffer_address_info{
        .buffer = index_buffer,
    };
    vk::DeviceAddress index_address = device.get().getBufferAddress(index_buffer_address_info);

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

    auto BLAS_build_sizes_info = device.get().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        BLAS_build_geometry_info,
        BLAS_max_primitive_counts);

    VmaAllocation BLAS_allocation;

    auto BLAS_buffer = create_buffer(allocator,
                                     BLAS_build_sizes_info.accelerationStructureSize,
                                     vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                                     vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                     BLAS_allocation,
                                     0);

    vk::AccelerationStructureCreateInfoKHR BLAS_create_info{
        .buffer = BLAS_buffer,
        .size = BLAS_build_sizes_info.accelerationStructureSize,
        .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
    };

    vk::CommandPoolCreateInfo cmd_pool_create_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = device.get_queue_family_index(),
    };
    auto cmd_pool = vk::raii::CommandPool(device.get(), cmd_pool_create_info);

    auto single_time_cmd_buffer = begin_single_time_commands(device.get(), cmd_pool);

    constexpr int frames_in_flight = 2;

    auto BLAS = device.get().createAccelerationStructureKHR(BLAS_create_info);

    // Build Bottom Level Acceleration Structure

    VmaAllocation BLAS_scratch_allocation;

    auto BLAS_scratch_buffer = create_buffer(allocator,
                                             BLAS_build_sizes_info.buildScratchSize,
                                             vk::BufferUsageFlagBits::eStorageBuffer |
                                             vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                             BLAS_scratch_allocation,
                                             VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

    vk::BufferDeviceAddressInfo BLAS_scratch_buffer_device_address_info{
        .buffer = BLAS_scratch_buffer,
    };
    auto BLAS_scratch_buffer_device_address = device.get().getBufferAddress(BLAS_scratch_buffer_device_address_info);

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
    auto BLAS_device_address = device.get().getAccelerationStructureAddressKHR(BLAS_device_address_info);

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

    auto BLAS_instance_buffer = create_buffer(allocator,
                                              sizeof(vk::AccelerationStructureInstanceKHR),
                                              vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                                              vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                              BLAS_instance_allocation,
                                              VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                              VMA_ALLOCATION_CREATE_MAPPED_BIT);

    {
        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(allocator, BLAS_instance_allocation, &allocation_info);
        memcpy(allocation_info.pMappedData, &BLAS_instance, sizeof(vk::AccelerationStructureInstanceKHR));
    }

    vk::BufferDeviceAddressInfo BLAS_instance_buffer_address_info{
        .buffer = BLAS_instance_buffer
    };
    auto BLAS_instance_device_address = device.get().getBufferAddress(BLAS_instance_buffer_address_info);

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

    auto TLAS_build_sizes_info = device.get().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        TLAS_build_geometry_info,
        TLAS_max_primitive_counts);

    VmaAllocation TLAS_allocation;

    auto TLAS_buffer = create_buffer(allocator,
                                     TLAS_build_sizes_info.accelerationStructureSize,
                                     vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
                                     /*    | vk::BufferUsageFlagBits::eShaderDeviceAddress*/,
                                     TLAS_allocation,
                                     0);

    vk::AccelerationStructureCreateInfoKHR TLAS_create_info{
        .buffer = TLAS_buffer,
        .size = TLAS_build_sizes_info.accelerationStructureSize,
        .type = vk::AccelerationStructureTypeKHR::eTopLevel,
    };

    auto TLAS = device.get().createAccelerationStructureKHR(TLAS_create_info);

    // Build Top Level Acceleration Structure

    VmaAllocation TLAS_scratch_allocation;

    auto TLAS_scratch_buffer = create_buffer(allocator,
                                             TLAS_build_sizes_info.buildScratchSize,
                                             vk::BufferUsageFlagBits::eStorageBuffer |
                                             vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                             TLAS_scratch_allocation,
                                             VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

    vk::BufferDeviceAddressInfo TLAS_scratch_buffer_device_address_info{
        .buffer = TLAS_scratch_buffer,
    };
    auto TLAS_scratch_buffer_device_address = device.get().getBufferAddress(TLAS_scratch_buffer_device_address_info);

    TLAS_build_geometry_info.dstAccelerationStructure = TLAS;
    TLAS_build_geometry_info.scratchData = TLAS_scratch_buffer_device_address;

    single_time_cmd_buffer.buildAccelerationStructuresKHR({TLAS_build_geometry_info}, {&TLAS_build_range_info});

    // Ray Trace Image

    auto queue_family_index_u32 = device.get_queue_family_index();

    vk::ImageCreateInfo ray_trace_image_create_info{
        .imageType = vk::ImageType::e2D,
        .format = sRGB_to_UNorm(swapchain_surface_format.format),
        .extent = vk::Extent3D{swapchain_extent.width, swapchain_extent.height, 1},
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
    auto ray_trace_image_view = device.get().createImageView(ray_trace_image_view_create_info);

    layout_transition(single_time_cmd_buffer, ray_trace_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

    end_single_time_commands(device.get_queue(), single_time_cmd_buffer);

    // Push Descriptors & Pipeline Layout

    std::vector<vk::DescriptorSetLayoutBinding> bindings;

    vk::DescriptorSetLayoutBinding AS_binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    };
    bindings.push_back(AS_binding);

    vk::DescriptorSetLayoutBinding image_binding{
        .binding = 1,
        .descriptorType = vk::DescriptorType::eStorageImage,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    };
    bindings.push_back(image_binding);

    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptor,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    auto descriptor_set_layout = device.get().createDescriptorSetLayout(descriptor_set_layout_create_info);

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

    auto pipeline_layout = device.get().createPipelineLayout(pipeline_layout_create_info);

    // Ray Tracing Pipeline

    auto ray_gen_shader_code = read_file("../src/shaders/spirv/raytrace.rgen.spv");
    vk::ShaderModuleCreateInfo ray_gen_shader_module_create_info{
        .codeSize = ray_gen_shader_code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(ray_gen_shader_code.data()),
    };
    auto ray_gen_shader_module = device.get().createShaderModule(ray_gen_shader_module_create_info);

    auto ray_miss_shader_code = read_file("../src/shaders/spirv/raytrace.rmiss.spv");
    vk::ShaderModuleCreateInfo ray_miss_shader_module_create_info{
        .codeSize = ray_miss_shader_code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(ray_miss_shader_code.data()),
    };
    auto ray_miss_shader_module = device.get().createShaderModule(ray_miss_shader_module_create_info);

    auto closest_hit_shader_code = read_file("../src/shaders/spirv/raytrace.rchit.spv");
    vk::ShaderModuleCreateInfo closest_hit_shader_module_create_info{
        .codeSize = closest_hit_shader_code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(closest_hit_shader_code.data()),
    };
    auto closest_hit_shader_module = device.get().createShaderModule(closest_hit_shader_module_create_info);

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

    auto ray_tracing_pipeline = device.get().createRayTracingPipelineKHR(nullptr, nullptr, ray_tracing_pipeline_create_info);

    // Shader Binding Table

    vk::StructureChain<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>
        ray_tracing_pipeline_properties_chain =
            adapter.get().getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    auto ray_tracing_pipeline_properties = ray_tracing_pipeline_properties_chain.get<
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    uint32_t handle_size = ray_tracing_pipeline_properties.shaderGroupHandleSize;
    uint32_t handle_alignment = ray_tracing_pipeline_properties.shaderGroupHandleAlignment;
    uint32_t base_alignment = ray_tracing_pipeline_properties.shaderGroupBaseAlignment;
    uint32_t group_count = ray_tracing_pipeline_create_info.groupCount;

    size_t data_size = handle_size * group_count;

    std::vector<uint8_t> shader_handles;
    shader_handles.reserve(data_size);

    shader_handles = ray_tracing_pipeline.getRayTracingShaderGroupHandlesKHR<uint8_t>(0, group_count, data_size);

    auto align_up = [](uint32_t size, uint32_t alignment) {
        return (size + alignment - 1) & ~(alignment - 1);
    };
    uint32_t rgen_size = align_up(handle_size, handle_alignment);
    uint32_t rmiss_size = align_up(handle_size, handle_alignment);
    uint32_t rchit_size = align_up(handle_size, handle_alignment);
    uint32_t callable_size = 0;

    uint32_t rmiss_offset = align_up(rgen_size, base_alignment);
    uint32_t rchit_offset = align_up(rmiss_offset + rmiss_size, base_alignment);
    uint32_t callable_offset = align_up(rchit_offset + rchit_size, base_alignment);

    size_t buffer_size = callable_offset + callable_size;

    vk::StridedDeviceAddressRegionKHR rgen_region{};
    vk::StridedDeviceAddressRegionKHR rmiss_region{};
    vk::StridedDeviceAddressRegionKHR rchit_region{};

    VmaAllocation SBT_allocation;

    vk::Buffer SBT_buffer = create_buffer(allocator,
                                          buffer_size,
                                          vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                          vk::BufferUsageFlagBits::eShaderBindingTableKHR,
                                          SBT_allocation,
                                          VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    {
        uint32_t rgen_offset = 0;

        vk::BufferDeviceAddressInfo SBT_buffer_address_info{
            .buffer = SBT_buffer,
        };
        vk::DeviceAddress SBT_address = device.get().getBufferAddress(
            SBT_buffer_address_info);

        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(allocator, SBT_allocation, &allocation_info);

        auto* p_data = static_cast<uint8_t*>(allocation_info.pMappedData);

        memcpy(p_data + rgen_offset, shader_handles.data() + 0 * handle_size, handle_size);
        rgen_region.deviceAddress = SBT_address + rgen_offset;
        rgen_region.stride = rgen_size;
        rgen_region.size = rgen_size;

        memcpy(p_data + rmiss_offset, shader_handles.data() + 1 * handle_size, handle_size);
        rmiss_region.deviceAddress = SBT_address + rmiss_offset;
        rmiss_region.stride = rmiss_size;
        rmiss_region.size = rmiss_size;

        memcpy(p_data + rchit_offset, shader_handles.data() + 2 * handle_size, handle_size);
        rchit_region.deviceAddress = SBT_address + rchit_offset;
        rchit_region.stride = rchit_size;
        rchit_region.size = rchit_size;
    }

    std::vector<vk::raii::Semaphore> image_available_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;
    std::vector<vk::raii::Fence> render_fences;

    vk::CommandBufferAllocateInfo cmd_buffer_allocate_info{
        .commandPool = cmd_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = frames_in_flight,
    };
    auto cmd_buffers = vk::raii::CommandBuffers(device.get(), cmd_buffer_allocate_info);

    for (size_t i = 0; i < frames_in_flight; ++i) {
        vk::FenceCreateInfo fence_create_info{
            .flags = vk::FenceCreateFlagBits::eSignaled
        };
        render_fences.emplace_back(device.get(), fence_create_info);

        vk::SemaphoreCreateInfo semaphore_info;
        image_available_semaphores.emplace_back(device.get(), semaphore_info);
    }

    for (size_t i = 0; i < swapchain_images.size(); ++i) {
        vk::SemaphoreCreateInfo semaphore_info;
        render_finished_semaphores.emplace_back(device.get(), semaphore_info);
    }

    uint32_t frame_index = 0;

    device.get_queue().waitIdle();

    while (!glfwWindowShouldClose(window)) {
        (void) device.get().waitForFences({*render_fences[frame_index]}, vk::True, std::numeric_limits<uint64_t>::max());
        device.get().resetFences({*render_fences[frame_index]});

        uint32_t image_index;

        auto& current_image_available_semaphore = image_available_semaphores[frame_index];

        vk::AcquireNextImageInfoKHR acquire_info{
            .swapchain = swapchain,
            .timeout = std::numeric_limits<uint64_t>::max(),
            .semaphore = current_image_available_semaphore,
            .deviceMask = 1,
        };
        image_index = device.get().acquireNextImage2KHR(acquire_info).value;
        auto swapchain_image = swapchain_images[image_index];

        auto& current_render_finished_semaphore = render_finished_semaphores[image_index];

        auto& cmd_buffer = cmd_buffers[frame_index];

        cmd_buffer.begin({});
        cmd_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, ray_tracing_pipeline);
        cmd_buffer.pushDescriptorSet(vk::PipelineBindPoint::eRayTracingKHR, pipeline_layout, 0, writes);
        cmd_buffer.traceRaysKHR(rgen_region, rmiss_region, rchit_region, {}, WIDTH, HEIGHT, 1);

        layout_transition(cmd_buffer, ray_trace_image, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
        layout_transition(cmd_buffer, swapchain_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        vk::ImageCopy copy_region{
            .srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            .dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            .extent = vk::Extent3D{swapchain_extent.width, swapchain_extent.height, 1},
        };

        cmd_buffer.copyImage(ray_trace_image,
                             vk::ImageLayout::eTransferSrcOptimal,
                             swapchain_image,
                             vk::ImageLayout::eTransferDstOptimal,
                             copy_region);

        layout_transition(cmd_buffer, swapchain_image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);
        layout_transition(cmd_buffer, ray_trace_image, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral);

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

        device.get_queue().submit(render_submit_info, *render_fences[frame_index]);

        vk::PresentInfoKHR present_info{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*current_render_finished_semaphore,
            .swapchainCount = 1,
            .pSwapchains = &*swapchain,
            .pImageIndices = &image_index,
        };

        (void) device.get_queue().presentKHR(present_info);

        static auto last_update_time = std::chrono::high_resolution_clock::now();
        static int frame_count = 0;

        frame_count++;

        auto current_time = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> elapsed_time = current_time - last_update_time;

        if (elapsed_time.count() >= 1.0) {
            double fps = static_cast<double>(frame_count) / elapsed_time.count();

            spdlog::info("{:.0f} fps ({:.2f} ms)", fps, 1000.0 / fps);

            frame_count = 0;
            last_update_time = current_time;
        }

        glfwPollEvents();

        frame_index = (frame_index + 1) % frames_in_flight;
    }

    device.get().waitIdle();

    vmaDestroyImage(allocator, ray_trace_image, ray_trace_image_allocation);
    vmaDestroyBuffer(allocator, SBT_buffer, SBT_allocation);
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "HWRT", nullptr, nullptr);

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