#include "renderer.h"

#include <iostream>
#include <fstream>

#include "window.h"
#include "spdlog/spdlog.h"
#include "vulkan/utils.h"

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
vk::Format srgb_to_unorm(const vk::Format format) {
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

Renderer::Renderer(const bool validation) {
    SCOPED_TIMER();

    ctx = std::make_unique<Context>(Window::get(), validation);

    auto swapchain_image_handles = ctx->get_swapchain().get_images();

    std::vector<Image> swapchain_images;
    swapchain_images.reserve(swapchain_image_handles.size());

    for (auto handle : swapchain_image_handles) {
        swapchain_images.emplace_back(handle);
    }

    auto adapter_props_chain = ctx->get_adapter().get().getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR,
        vk::PhysicalDeviceAccelerationStructurePropertiesKHR
    >();

    auto rt_pipeline_props = adapter_props_chain.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
    auto as_props = adapter_props_chain.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();

    auto vertex_buffer = BufferBuilder()
                         .size(sizeof(Vertex) * vertices.size())
                         .usage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                                vk::BufferUsageFlagBits::eShaderDeviceAddress |
                                vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR)
                         .allocation_flags(
                             VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                         .build(ctx->get_allocator());

    memcpy(vertex_buffer.mapped_ptr(), vertices.data(), sizeof(Vertex) * vertices.size());

    auto index_buffer = BufferBuilder()
                        .size(sizeof(uint32_t) * indices.size())
                        .usage(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                               vk::BufferUsageFlagBits::eShaderDeviceAddress |
                               vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR)
                        .allocation_flags(
                            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                        .build(ctx->get_allocator());

    memcpy(index_buffer.mapped_ptr(), indices.data(), sizeof(u_int32_t) * indices.size());

    auto vertex_address = vertex_buffer.get_device_address(ctx->get_device());
    auto index_address = index_buffer.get_device_address(ctx->get_device());

    // Create Bottom Level Acceleration Structure

    auto triangle_count = static_cast<uint32_t>(indices.size() / 3);

    vk::AccelerationStructureGeometryTrianglesDataKHR blas_geometry_triangles_data{
        .vertexFormat = vk::Format::eR32G32B32Sfloat,
        .vertexData = vertex_address,
        .vertexStride = sizeof(Vertex),
        .maxVertex = static_cast<uint32_t>(vertices.size() - 1),
        .indexType = vk::IndexType::eUint32,
        .indexData = index_address,
    };

    vk::AccelerationStructureGeometryKHR blas_geometry{
        .geometryType = vk::GeometryTypeKHR::eTriangles,
        .geometry = blas_geometry_triangles_data,
        .flags = vk::GeometryFlagBitsKHR::eOpaque,
    };

    vk::AccelerationStructureBuildRangeInfoKHR blas_build_range_info{
        .primitiveCount = triangle_count,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    };

    vk::AccelerationStructureBuildGeometryInfoKHR blas_build_geometry_info{
        .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries = &blas_geometry,
    };

    std::vector<uint32_t> blas_max_primitive_counts(1);
    blas_max_primitive_counts[0] = blas_build_range_info.primitiveCount;

    auto blas_build_sizes_info = ctx->get_device().get().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        blas_build_geometry_info,
        blas_max_primitive_counts);

    auto blas_buffer = BufferBuilder()
                       .size(blas_build_sizes_info.accelerationStructureSize)
                       .usage(
                           vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                           vk::BufferUsageFlagBits::eShaderDeviceAddress)
                       .build(ctx->get_allocator());

    vk::AccelerationStructureCreateInfoKHR blas_create_info{
        .buffer = blas_buffer.get(),
        .size = blas_build_sizes_info.accelerationStructureSize,
        .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
    };

    auto single_time_encoder = SingleTimeEncoder(ctx->get_device());

    auto blas = ctx->get_device().get().createAccelerationStructureKHR(blas_create_info);

    // Build Bottom Level Acceleration Structure

    auto scratch_alignment = as_props.minAccelerationStructureScratchOffsetAlignment;

    auto blas_scratch_buffer = BufferBuilder()
                               .size(blas_build_sizes_info.buildScratchSize)
                               .usage(
                                   vk::BufferUsageFlagBits::eStorageBuffer |
                                   vk::BufferUsageFlagBits::eShaderDeviceAddress)
                               .min_alignment(scratch_alignment)
                               .build(ctx->get_allocator());

    auto blas_scratch_buffer_device_address = blas_scratch_buffer.get_device_address(ctx->get_device());

    blas_build_geometry_info.dstAccelerationStructure = blas;
    blas_build_geometry_info.scratchData = blas_scratch_buffer_device_address;

    single_time_encoder.get_cmd().buildAccelerationStructuresKHR({blas_build_geometry_info}, {&blas_build_range_info});

    vk::MemoryBarrier2 as_build_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        .srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
        .dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
        .dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR,
    };

    vk::DependencyInfo dependency_info_as_build{
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &as_build_barrier,
    };

    single_time_encoder.get_cmd().pipelineBarrier2(dependency_info_as_build);

    // Create Top Level Acceleration Structure

    vk::AccelerationStructureDeviceAddressInfoKHR blas_device_address_info{
        .accelerationStructure = blas
    };
    auto blas_device_address = ctx->get_device().get().getAccelerationStructureAddressKHR(blas_device_address_info);

    vk::TransformMatrixKHR transform{
        std::array<std::array<float, 4>, 3>{
            {
                {{1.f, 0.f, 0.f, 0.f}},
                {{0.f, 1.f, 0.f, 0.f}},
                {{0.f, 0.f, 1.f, 0.f}},
            }
        }
    };

    vk::AccelerationStructureInstanceKHR blas_instance{
        .transform = transform,
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = blas_device_address
    };

    auto blas_instance_buffer = BufferBuilder()
                                .size(sizeof(vk::AccelerationStructureInstanceKHR))
                                .usage(
                                    vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                                    vk::BufferUsageFlagBits::eShaderDeviceAddress)
                                .allocation_flags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                                  VMA_ALLOCATION_CREATE_MAPPED_BIT)
                                .build(ctx->get_allocator());

    memcpy(blas_instance_buffer.mapped_ptr(), &blas_instance, sizeof(vk::AccelerationStructureInstanceKHR));

    auto blas_instance_device_address = blas_instance_buffer.get_device_address(ctx->get_device());

    vk::AccelerationStructureGeometryInstancesDataKHR tlas_geometry_instances_data{
        .arrayOfPointers = vk::False,
        .data = blas_instance_device_address,
    };

    vk::AccelerationStructureGeometryKHR tlas_geometry{
        .geometryType = vk::GeometryTypeKHR::eInstances,
        .geometry = tlas_geometry_instances_data,
        .flags = vk::GeometryFlagBitsKHR::eOpaque,
    };

    vk::AccelerationStructureBuildRangeInfoKHR tlas_build_range_info{
        .primitiveCount = 1,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    };

    vk::AccelerationStructureBuildGeometryInfoKHR tlas_build_geometry_info{
        .type = vk::AccelerationStructureTypeKHR::eTopLevel,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries = &tlas_geometry,
    };

    std::vector<uint32_t> tlas_max_primitive_counts(1);
    tlas_max_primitive_counts[0] = tlas_build_range_info.primitiveCount;

    auto tlas_build_sizes_info = ctx->get_device().get().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        tlas_build_geometry_info,
        tlas_max_primitive_counts);

    auto tlas_buffer = BufferBuilder()
                       .size(tlas_build_sizes_info.accelerationStructureSize)
                       .usage(
                           vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR)
                       .build(ctx->get_allocator());

    vk::AccelerationStructureCreateInfoKHR tlas_create_info{
        .buffer = tlas_buffer.get(),
        .size = tlas_build_sizes_info.accelerationStructureSize,
        .type = vk::AccelerationStructureTypeKHR::eTopLevel,
    };

    auto tlas = ctx->get_device().get().createAccelerationStructureKHR(tlas_create_info);

    // Build Top Level Acceleration Structure

    auto tlas_scratch_buffer = BufferBuilder()
                               .size(tlas_build_sizes_info.buildScratchSize)
                               .usage(
                                   vk::BufferUsageFlagBits::eStorageBuffer |
                                   vk::BufferUsageFlagBits::eShaderDeviceAddress)
                               .min_alignment(scratch_alignment)
                               .build(ctx->get_allocator());

    auto tlas_scratch_buffer_device_address = tlas_scratch_buffer.get_device_address(ctx->get_device());

    tlas_build_geometry_info.dstAccelerationStructure = *tlas;
    tlas_build_geometry_info.scratchData = tlas_scratch_buffer_device_address;

    single_time_encoder.get_cmd().buildAccelerationStructuresKHR({tlas_build_geometry_info}, {&tlas_build_range_info});

    // Ray Trace Image

    auto queue_family_index_u32 = ctx->get_device().get_queue_family_index();

    vk::ImageCreateInfo rt_image_create_info{
        .imageType = vk::ImageType::e2D,
        .format = srgb_to_unorm(ctx->get_swapchain().get_surface_format().format),
        .extent = vk::Extent3D{ctx->get_swapchain().get_extent().width, ctx->get_swapchain().get_extent().height, 1},
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

    auto rt_image = Image(rt_image_create_info, ctx->get_allocator());

    vk::ImageViewCreateInfo rt_image_view_create_info{
        .image = rt_image.get(),
        .viewType = vk::ImageViewType::e2D,
        .format = srgb_to_unorm(ctx->get_swapchain().get_surface_format().format),
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    };
    auto rt_image_view = ctx->get_device().get().createImageView(rt_image_view_create_info);

    rt_image.transition_layout(single_time_encoder.get_cmd(),
                               vk::ImageLayout::eGeneral,
                               vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                               vk::AccessFlagBits2::eShaderWrite);

    single_time_encoder.submit(ctx->get_device().get_queue());

    // Push Descriptors & Pipeline Layout

    std::vector<vk::DescriptorSetLayoutBinding> bindings;

    vk::DescriptorSetLayoutBinding as_binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    };
    bindings.push_back(as_binding);

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

    auto descriptor_set_layout = ctx->get_device().get().createDescriptorSetLayout(descriptor_set_layout_create_info);

    vk::PipelineLayoutCreateInfo pipeline_layout_create_info{
        .setLayoutCount = 1,
        .pSetLayouts = &*descriptor_set_layout,
    };

    auto rt_pipeline_layout = ctx->get_device().get().createPipelineLayout(pipeline_layout_create_info);

    // Ray Tracing Pipeline

    auto rgen_shader_code = read_file("../src/shaders/spirv/raytrace.rgen.spv");
    vk::ShaderModuleCreateInfo rgen_shader_module_create_info{
        .codeSize = rgen_shader_code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(rgen_shader_code.data()),
    };
    auto rgen_shader_module = ctx->get_device().get().createShaderModule(rgen_shader_module_create_info);

    auto rmiss_shader_code = read_file("../src/shaders/spirv/raytrace.rmiss.spv");
    vk::ShaderModuleCreateInfo rmiss_shader_module_create_info{
        .codeSize = rmiss_shader_code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(rmiss_shader_code.data()),
    };
    auto rmiss_shader_module = ctx->get_device().get().createShaderModule(rmiss_shader_module_create_info);

    auto rchit_shader_code = read_file("../src/shaders/spirv/raytrace.rchit.spv");
    vk::ShaderModuleCreateInfo rchit_shader_module_create_info{
        .codeSize = rchit_shader_code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(rchit_shader_code.data()),
    };
    auto rchit_shader_module = ctx->get_device().get().createShaderModule(rchit_shader_module_create_info);

    std::vector shader_stage_create_info_list = {
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eRaygenKHR,
            .module = rgen_shader_module,
            .pName = "main",
        },
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eMissKHR,
            .module = rmiss_shader_module,
            .pName = "main",
        },
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eClosestHitKHR,
            .module = rchit_shader_module,
            .pName = "main",
        },
    };

    std::vector rt_shader_groups = {
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

    vk::RayTracingPipelineCreateInfoKHR rt_pipeline_create_info{
        .stageCount = static_cast<uint32_t>(shader_stage_create_info_list.size()),
        .pStages = shader_stage_create_info_list.data(),
        .groupCount = static_cast<uint32_t>(rt_shader_groups.size()),
        .pGroups = rt_shader_groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = *rt_pipeline_layout,
    };
    auto rt_pipeline = ctx->get_device().get().createRayTracingPipelineKHR(nullptr, nullptr, rt_pipeline_create_info);

    // Shader Binding Table

    uint32_t handle_size = rt_pipeline_props.shaderGroupHandleSize;
    uint32_t handle_alignment = rt_pipeline_props.shaderGroupHandleAlignment;
    uint32_t base_alignment = rt_pipeline_props.shaderGroupBaseAlignment;
    uint32_t handle_count = rt_pipeline_create_info.groupCount;

    // TODO: to function
    //////////////////////////////////////////////////
    size_t data_size = handle_size * handle_count;

    std::vector<uint8_t> shader_handles = rt_pipeline.getRayTracingShaderGroupHandlesKHR<uint8_t>(0, handle_count, data_size);

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
    ////////////////////////////////////////////////

    vk::StridedDeviceAddressRegionKHR rgen_region{};
    vk::StridedDeviceAddressRegionKHR rmiss_region{};
    vk::StridedDeviceAddressRegionKHR rchit_region{};

    auto sbt_buffer = BufferBuilder()
                      .size(buffer_size)
                      .usage(
                          vk::BufferUsageFlagBits::eShaderDeviceAddress |
                          vk::BufferUsageFlagBits::eShaderBindingTableKHR)
                      .allocation_flags(VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                      .build(ctx->get_allocator());

    {
        auto sbt_address = sbt_buffer.get_device_address(ctx->get_device());

        auto* p_data = sbt_buffer.mapped_ptr<uint8_t>();

        memcpy(p_data + rgen_offset, shader_handles.data() + 0 * handle_size, handle_size);
        rgen_region.deviceAddress = sbt_address + rgen_offset;
        rgen_region.stride = rgen_size;
        rgen_region.size = rgen_size;

        memcpy(p_data + rmiss_offset, shader_handles.data() + 1 * handle_size, handle_size);
        rmiss_region.deviceAddress = sbt_address + rmiss_offset;
        rmiss_region.stride = rmiss_size;
        rmiss_region.size = rmiss_size;

        memcpy(p_data + rchit_offset, shader_handles.data() + 2 * handle_size, handle_size);
        rchit_region.deviceAddress = sbt_address + rchit_offset;
        rchit_region.stride = rchit_size;
        rchit_region.size = rchit_size;
    }

    constexpr int frames_in_flight = 2;
    const uint32_t swapchain_image_count = ctx->get_swapchain().get_images().size();

    encoder = std::make_unique<Encoder>(ctx->get_device(), frames_in_flight);
    frame_mgr = std::make_unique<FrameManager>(ctx->get_device(), frames_in_flight, swapchain_image_count);

    ctx->get_device().get_queue().waitIdle();

    res = std::make_unique<Resources>(Resources{
        .vertex_buffer = std::move(vertex_buffer),
        .index_buffer = std::move(index_buffer),
        .blas_buffer = std::move(blas_buffer),
        .tlas_buffer = std::move(tlas_buffer),
        .blas = std::move(blas),
        .tlas = std::move(tlas),
        .sbt_buffer = std::move(sbt_buffer),
        .rgen_region = rgen_region,
        .rmiss_region = rmiss_region,
        .rchit_region = rchit_region,
        .descriptor_set_layout = std::move(descriptor_set_layout),
        .rt_pipeline_layout = std::move(rt_pipeline_layout),
        .rt_pipeline = std::move(rt_pipeline),
        .rt_image = std::move(rt_image),
        .rt_image_view = std::move(rt_image_view),
        .swapchain_images = std::move(swapchain_images),
    });
}

void Renderer::draw_frame() const {
    (void) ctx->get_device().get().
                waitForFences({frame_mgr->get_in_flight_fence()},
                              vk::True,
                              std::numeric_limits<uint64_t>::max());
    ctx->get_device().get().resetFences({frame_mgr->get_in_flight_fence()});

    uint32_t image_index;

    vk::AcquireNextImageInfoKHR acquire_info{
        .swapchain = ctx->get_swapchain().get(),
        .timeout = std::numeric_limits<uint64_t>::max(),
        .semaphore = frame_mgr->get_image_available_semaphore(),
        .deviceMask = 1,
    };
    image_index = ctx->get_device().get().acquireNextImage2KHR(acquire_info).value;

    encoder->begin(frame_mgr->get_frame_index());
    auto& cmd = encoder->get_cmd();

    vk::WriteDescriptorSetAccelerationStructureKHR write_as_info{
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &*res->tlas,
    };

    vk::WriteDescriptorSet write_as{
        .pNext = &write_as_info,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
    };

    vk::DescriptorImageInfo descriptor_image_info{
        .imageView = res->rt_image_view,
        .imageLayout = vk::ImageLayout::eGeneral,
    };

    vk::WriteDescriptorSet write_image{
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eStorageImage,
        .pImageInfo = &descriptor_image_info,
    };

    std::vector writes{write_as, write_image};

    cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, res->rt_pipeline);
    cmd.pushDescriptorSet(vk::PipelineBindPoint::eRayTracingKHR, res->rt_pipeline_layout, 0, writes);

    cmd.traceRaysKHR(res->rgen_region,
                     res->rmiss_region,
                     res->rchit_region,
                     {},
                     Window::get_width(),
                     Window::get_height(),
                     1);

    res->rt_image.transition_layout(cmd,
                                    vk::ImageLayout::eTransferSrcOptimal,
                                    vk::PipelineStageFlagBits2::eTransfer,
                                    vk::AccessFlagBits2::eTransferRead);

    res->swapchain_images[image_index].transition_layout(cmd,
                                                         vk::ImageLayout::eTransferDstOptimal,
                                                         vk::PipelineStageFlagBits2::eTransfer,
                                                         vk::AccessFlagBits2::eTransferWrite);

    vk::ImageCopy copy_region{
        .srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .extent = vk::Extent3D{ctx->get_swapchain().get_extent().width,
                               ctx->get_swapchain().get_extent().height, 1},
    };

    cmd.copyImage(res->rt_image.get(),
                  vk::ImageLayout::eTransferSrcOptimal,
                  res->swapchain_images[image_index].get(),
                  vk::ImageLayout::eTransferDstOptimal,
                  copy_region);

    res->swapchain_images[image_index].transition_layout(cmd,
                                                         vk::ImageLayout::ePresentSrcKHR,
                                                         vk::PipelineStageFlagBits2::eBottomOfPipe,
                                                         vk::AccessFlagBits2::eNone);

    res->rt_image.transition_layout(cmd,
                                    vk::ImageLayout::eGeneral,
                                    vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                                    vk::AccessFlagBits2::eShaderWrite);

    encoder->end();

    vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eTopOfPipe;

    vk::SubmitInfo submit_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*frame_mgr->get_image_available_semaphore(),
        .pWaitDstStageMask = &wait_stage,

        .commandBufferCount = 1,
        .pCommandBuffers = &*cmd,

        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*frame_mgr->get_render_finished_semaphore(image_index),
    };

    ctx->get_device().get_queue().submit(submit_info, frame_mgr->get_in_flight_fence());

    vk::PresentInfoKHR present_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*frame_mgr->get_render_finished_semaphore(image_index),
        .swapchainCount = 1,
        .pSwapchains = &*ctx->get_swapchain().get(),
        .pImageIndices = &image_index,
    };

    (void) ctx->get_device().get_queue().presentKHR(present_info);

    frame_mgr->update();
}


Context& Renderer::get_ctx() const {
    return *ctx;
}

Resources& Renderer::get_res() const {
    return *res;
}

Encoder& Renderer::get_encoder() const {
    return *encoder;
}

FrameManager& Renderer::get_frame_mgr() const {
    return *frame_mgr;
}