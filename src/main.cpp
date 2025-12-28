#include <iostream>
#include <fstream>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include "context.h"
#include "frame.h"
#include "vulkan/adapter.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/encoder.h"
#include "vulkan/allocator.h"
#include "vulkan/buffer.h"
#include "vulkan/image.h"

#define WIDTH 800
#define HEIGHT 600

// TODO: Abstractions (buffer and image builders, pipeline, blas, tlas, sbt, window, input)

const std::vector validation_layers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool validation = false;
#else
constexpr bool validation = true;
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

// TODO: is it necessary?
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

void layout_transition(const vk::raii::CommandBuffer& cmd_buffer, const vk::Image image, const vk::ImageLayout old_layout,
                       const vk::ImageLayout new_layout) {
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

void init_vulkan(GLFWwindow* window) {
    auto ctx = Context(window, validation);

    auto vertex_buffer = Buffer(ctx.get_allocator(),
                                sizeof(Vertex) * vertices.size(),
                                vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                                vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    {
        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(ctx.get_allocator().get(), vertex_buffer.get_allocation(), &allocation_info);
        memcpy(allocation_info.pMappedData, vertices.data(), sizeof(Vertex) * vertices.size());
    }

    auto index_buffer = Buffer(ctx.get_allocator(),
                               sizeof(uint32_t) * indices.size(),
                               vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                               vk::BufferUsageFlagBits::eShaderDeviceAddress |
                               vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                               VMA_ALLOCATION_CREATE_MAPPED_BIT);

    {
        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(ctx.get_allocator().get(), index_buffer.get_allocation(), &allocation_info);
        memcpy(allocation_info.pMappedData, indices.data(), sizeof(u_int32_t) * indices.size());
    }

    vk::BufferDeviceAddressInfo vertex_buffer_address_info{
        .buffer = vertex_buffer.get(),
    };
    vk::DeviceAddress vertex_address = ctx.get_device().get().getBufferAddress(vertex_buffer_address_info);

    vk::BufferDeviceAddressInfo index_buffer_address_info{
        .buffer = index_buffer.get(),
    };
    vk::DeviceAddress index_address = ctx.get_device().get().getBufferAddress(index_buffer_address_info);

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

    auto BLAS_build_sizes_info = ctx.get_device().get().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        BLAS_build_geometry_info,
        BLAS_max_primitive_counts);

    auto BLAS_buffer = Buffer(ctx.get_allocator(),
                              BLAS_build_sizes_info.accelerationStructureSize,
                              vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                              vk::BufferUsageFlagBits::eShaderDeviceAddress,
                              0);

    vk::AccelerationStructureCreateInfoKHR BLAS_create_info{
        .buffer = BLAS_buffer.get(),
        .size = BLAS_build_sizes_info.accelerationStructureSize,
        .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
    };

    auto single_time_encoder = SingleTimeEncoder(ctx.get_device());

    auto BLAS = ctx.get_device().get().createAccelerationStructureKHR(BLAS_create_info);

    // Build Bottom Level Acceleration Structure

    auto BLAS_scratch_buffer = Buffer(ctx.get_allocator(),
                                      BLAS_build_sizes_info.buildScratchSize,
                                      vk::BufferUsageFlagBits::eStorageBuffer |
                                      vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                      0);

    vk::BufferDeviceAddressInfo BLAS_scratch_buffer_device_address_info{
        .buffer = BLAS_scratch_buffer.get(),
    };
    auto BLAS_scratch_buffer_device_address = ctx.get_device().get().getBufferAddress(BLAS_scratch_buffer_device_address_info);

    BLAS_build_geometry_info.dstAccelerationStructure = BLAS;
    BLAS_build_geometry_info.scratchData = BLAS_scratch_buffer_device_address;

    single_time_encoder.get_cmd().buildAccelerationStructuresKHR({BLAS_build_geometry_info}, {&BLAS_build_range_info});

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

    single_time_encoder.get_cmd().pipelineBarrier2(dependency_info_as_build);

    // Create Top Level Acceleration Structure

    vk::AccelerationStructureDeviceAddressInfoKHR BLAS_device_address_info{
        .accelerationStructure = BLAS
    };
    auto BLAS_device_address = ctx.get_device().get().getAccelerationStructureAddressKHR(BLAS_device_address_info);

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

    auto BLAS_instance_buffer = Buffer(ctx.get_allocator(),
                                       sizeof(vk::AccelerationStructureInstanceKHR),
                                       vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                                       vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                       VMA_ALLOCATION_CREATE_MAPPED_BIT);

    {
        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(ctx.get_allocator().get(), BLAS_instance_buffer.get_allocation(), &allocation_info);
        memcpy(allocation_info.pMappedData, &BLAS_instance, sizeof(vk::AccelerationStructureInstanceKHR));
    }

    vk::BufferDeviceAddressInfo BLAS_instance_buffer_address_info{
        .buffer = BLAS_instance_buffer.get()
    };
    auto BLAS_instance_device_address = ctx.get_device().get().getBufferAddress(BLAS_instance_buffer_address_info);

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

    auto TLAS_build_sizes_info = ctx.get_device().get().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        TLAS_build_geometry_info,
        TLAS_max_primitive_counts);

    auto TLAS_buffer = Buffer(ctx.get_allocator(),
                              TLAS_build_sizes_info.accelerationStructureSize,
                              vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
                              /*    | vk::BufferUsageFlagBits::eShaderDeviceAddress*/,
                              0);

    vk::AccelerationStructureCreateInfoKHR TLAS_create_info{
        .buffer = TLAS_buffer.get(),
        .size = TLAS_build_sizes_info.accelerationStructureSize,
        .type = vk::AccelerationStructureTypeKHR::eTopLevel,
    };

    auto TLAS = ctx.get_device().get().createAccelerationStructureKHR(TLAS_create_info);

    // Build Top Level Acceleration Structure

    auto TLAS_scratch_buffer = Buffer(ctx.get_allocator(),
                                      TLAS_build_sizes_info.buildScratchSize,
                                      vk::BufferUsageFlagBits::eStorageBuffer |
                                      vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                      0);

    vk::BufferDeviceAddressInfo TLAS_scratch_buffer_device_address_info{
        .buffer = TLAS_scratch_buffer.get(),
    };
    auto TLAS_scratch_buffer_device_address = ctx.get_device().get().getBufferAddress(TLAS_scratch_buffer_device_address_info);

    TLAS_build_geometry_info.dstAccelerationStructure = TLAS;
    TLAS_build_geometry_info.scratchData = TLAS_scratch_buffer_device_address;

    single_time_encoder.get_cmd().buildAccelerationStructuresKHR({TLAS_build_geometry_info}, {&TLAS_build_range_info});

    // Ray Trace Image

    auto queue_family_index_u32 = ctx.get_device().get_queue_family_index();

    vk::ImageCreateInfo RT_image_create_info{
        .imageType = vk::ImageType::e2D,
        .format = sRGB_to_UNorm(ctx.get_swapchain().get_surface_format().format),
        .extent = vk::Extent3D{ctx.get_swapchain().get_extent().width, ctx.get_swapchain().get_extent().height, 1},
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

    auto RT_image = Image(RT_image_create_info, ctx.get_allocator());

    vk::ImageViewCreateInfo ray_trace_image_view_create_info{
        .image = RT_image.get(),
        .viewType = vk::ImageViewType::e2D,
        .format = sRGB_to_UNorm(ctx.get_swapchain().get_surface_format().format),
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    };
    auto RT_image_view = ctx.get_device().get().createImageView(ray_trace_image_view_create_info);

    layout_transition(single_time_encoder.get_cmd(),
                      RT_image.get(),
                      vk::ImageLayout::eUndefined,
                      vk::ImageLayout::eGeneral);

    single_time_encoder.submit(ctx.get_device().get_queue());

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

    auto descriptor_set_layout = ctx.get_device().get().createDescriptorSetLayout(descriptor_set_layout_create_info);

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
        .imageView = RT_image_view,
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

    auto pipeline_layout = ctx.get_device().get().createPipelineLayout(pipeline_layout_create_info);

    // Ray Tracing Pipeline

    auto ray_gen_shader_code = read_file("../src/shaders/spirv/raytrace.rgen.spv");
    vk::ShaderModuleCreateInfo ray_gen_shader_module_create_info{
        .codeSize = ray_gen_shader_code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(ray_gen_shader_code.data()),
    };
    auto ray_gen_shader_module = ctx.get_device().get().createShaderModule(ray_gen_shader_module_create_info);

    auto ray_miss_shader_code = read_file("../src/shaders/spirv/raytrace.rmiss.spv");
    vk::ShaderModuleCreateInfo ray_miss_shader_module_create_info{
        .codeSize = ray_miss_shader_code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(ray_miss_shader_code.data()),
    };
    auto ray_miss_shader_module = ctx.get_device().get().createShaderModule(ray_miss_shader_module_create_info);

    auto closest_hit_shader_code = read_file("../src/shaders/spirv/raytrace.rchit.spv");
    vk::ShaderModuleCreateInfo closest_hit_shader_module_create_info{
        .codeSize = closest_hit_shader_code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(closest_hit_shader_code.data()),
    };
    auto closest_hit_shader_module = ctx.get_device().get().createShaderModule(closest_hit_shader_module_create_info);

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

    std::vector RT_shader_groups = {
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

    vk::RayTracingPipelineCreateInfoKHR RT_pipeline_create_info{
        .stageCount = static_cast<uint32_t>(shader_stage_create_info_list.size()),
        .pStages = shader_stage_create_info_list.data(),
        .groupCount = static_cast<uint32_t>(RT_shader_groups.size()),
        .pGroups = RT_shader_groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = pipeline_layout,
    };

    auto RT_pipeline = ctx.get_device().get().createRayTracingPipelineKHR(nullptr, nullptr, RT_pipeline_create_info);

    // Shader Binding Table

    auto RT_pipeline_props_chain = ctx.get_adapter().get().getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR
    >();

    auto RT_pipeline_props = RT_pipeline_props_chain.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    uint32_t handle_size = RT_pipeline_props.shaderGroupHandleSize;
    uint32_t handle_alignment = RT_pipeline_props.shaderGroupHandleAlignment;
    uint32_t base_alignment = RT_pipeline_props.shaderGroupBaseAlignment;
    uint32_t handle_count = RT_pipeline_create_info.groupCount;

    // TODO: to function
    //////////////////////////////////////////////////
    size_t data_size = handle_size * handle_count;

    std::vector<uint8_t> shader_handles = RT_pipeline.getRayTracingShaderGroupHandlesKHR<uint8_t>(0, handle_count, data_size);

    auto align_up = [](uint32_t size, uint32_t alignment) {
        return (size + alignment - 1) & ~(alignment - 1);
    };
    uint32_t rgen_size = align_up(handle_size, handle_alignment);
    uint32_t rmiss_size = align_up(handle_size, handle_alignment);
    uint32_t rchit_size = align_up(handle_size, handle_alignment);
    uint32_t callable_size = 0;

    uint32_t rgen_offset = 0;
    uint32_t rmiss_offset = align_up(rgen_offset + rgen_size, base_alignment);
    uint32_t rchit_offset = align_up(rmiss_offset + rmiss_size, base_alignment);
    uint32_t callable_offset = align_up(rchit_offset + rchit_size, base_alignment);

    size_t buffer_size = callable_offset + callable_size;
    //////////////////////////////////////////////////

    vk::StridedDeviceAddressRegionKHR rgen_region{};
    vk::StridedDeviceAddressRegionKHR rmiss_region{};
    vk::StridedDeviceAddressRegionKHR rchit_region{};

    auto SBT_buffer = Buffer(ctx.get_allocator(),
                             buffer_size,
                             vk::BufferUsageFlagBits::eShaderDeviceAddress |
                             vk::BufferUsageFlagBits::eShaderBindingTableKHR,
                             VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    {
        vk::BufferDeviceAddressInfo SBT_buffer_address_info{
            .buffer = SBT_buffer.get(),
        };
        vk::DeviceAddress SBT_address = ctx.get_device().get().getBufferAddress(
            SBT_buffer_address_info);

        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(ctx.get_allocator().get(), SBT_buffer.get_allocation(), &allocation_info);

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

    constexpr int frames_in_flight = 2;
    const int swapchain_image_count = ctx.get_swapchain().get_images().size();

    auto encoder = Encoder(ctx.get_device(), frames_in_flight);
    auto frame_mgr = FrameManager(ctx.get_device(), frames_in_flight, swapchain_image_count);

    ctx.get_device().get_queue().waitIdle();

    while (!glfwWindowShouldClose(window)) {
        (void) ctx.get_device().get().
                   waitForFences({*frame_mgr.get_in_flight_fence()}, vk::True, std::numeric_limits<uint64_t>::max());
        ctx.get_device().get().resetFences({*frame_mgr.get_in_flight_fence()});

        uint32_t image_index;

        vk::AcquireNextImageInfoKHR acquire_info{
            .swapchain = ctx.get_swapchain().get(),
            .timeout = std::numeric_limits<uint64_t>::max(),
            .semaphore = frame_mgr.get_image_available_semaphore(),
            .deviceMask = 1,
        };
        image_index = ctx.get_device().get().acquireNextImage2KHR(acquire_info).value;
        auto swapchain_image = ctx.get_swapchain().get_images()[image_index];

        encoder.begin(frame_mgr.get_frame_index());
        auto& cmd = encoder.get_cmd();

        cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, RT_pipeline);
        cmd.pushDescriptorSet(vk::PipelineBindPoint::eRayTracingKHR, pipeline_layout, 0, writes);
        cmd.traceRaysKHR(rgen_region, rmiss_region, rchit_region, {}, WIDTH, HEIGHT, 1);

        layout_transition(cmd, RT_image.get(), vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
        layout_transition(cmd, swapchain_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        vk::ImageCopy copy_region{
            .srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            .dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            .extent = vk::Extent3D{ctx.get_swapchain().get_extent().width, ctx.get_swapchain().get_extent().height, 1},
        };

        cmd.copyImage(RT_image.get(),
                      vk::ImageLayout::eTransferSrcOptimal,
                      swapchain_image,
                      vk::ImageLayout::eTransferDstOptimal,
                      copy_region);

        layout_transition(cmd, swapchain_image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);
        layout_transition(cmd, RT_image.get(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral);

        encoder.end();

        vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eTopOfPipe;

        vk::SubmitInfo render_submit_info{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*frame_mgr.get_image_available_semaphore(),
            .pWaitDstStageMask = &wait_stage,

            .commandBufferCount = 1,
            .pCommandBuffers = &*cmd,

            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &*frame_mgr.get_render_finished_semaphore(image_index),
        };

        ctx.get_device().get_queue().submit(render_submit_info, *frame_mgr.get_in_flight_fence());

        vk::PresentInfoKHR present_info{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*frame_mgr.get_render_finished_semaphore(image_index),
            .swapchainCount = 1,
            .pSwapchains = &*ctx.get_swapchain().get(),
            .pImageIndices = &image_index,
        };

        (void) ctx.get_device().get_queue().presentKHR(present_info);

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

        frame_mgr.update();
    }

    ctx.get_device().get().waitIdle();
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