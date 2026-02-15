#include "renderer.h"

#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>

#include "glm/gtc/type_ptr.hpp"
#include "spdlog/spdlog.h"

#include "vulkan/pipeline.h"
#include "vulkan/utils.h"
#include "camera.h"
#include "window.h"

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

Renderer::Renderer(Context& ctx_) : ctx(ctx_) {
    SCOPED_TIMER();

    VkSurfaceKHR surface_;
    if (glfwCreateWindowSurface(*ctx.get_instance().get(), Window::get(), nullptr, &surface_) != 0) {
        throw std::runtime_error("Failed to create window surface");
    }
    auto surface = vk::raii::SurfaceKHR(ctx.get_instance().get(), surface_);

    swapchain = std::make_unique<Swapchain>(ctx.get_adapter(), ctx.get_device(), Window::get(), surface);

    auto rt_pipeline_props = ctx.get_adapter().get().getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR
    >().get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    auto single_time_encoder = SingleTimeEncoder(ctx.get_device());

    // Ray Trace Image

    auto rt_image = ImageBuilder()
                    .type(vk::ImageType::e2D)
                    .format(srgb_to_unorm(swapchain->get_surface_format().format))
                    .size(swapchain->get_extent().width, swapchain->get_extent().height)
                    .mip_levels(1)
                    .layers(1)
                    .samples(vk::SampleCountFlagBits::e1)
                    .usage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc)
                    .build(ctx.get_allocator());

    vk::ImageViewCreateInfo rt_image_view_create_info{
        .image = rt_image.get(),
        .viewType = vk::ImageViewType::e2D,
        .format = srgb_to_unorm(swapchain->get_surface_format().format),
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    };
    auto rt_image_view = ctx.get_device().get().createImageView(rt_image_view_create_info);

    rt_image.transition_layout(single_time_encoder.get_cmd(),
                               vk::ImageLayout::eGeneral,
                               vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                               vk::AccessFlagBits2::eShaderWrite);

    single_time_encoder.submit(ctx.get_device());

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

    vk::DescriptorSetLayoutBinding uniform_binding{
        .binding = 2,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    };
    bindings.push_back(uniform_binding);

    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptor,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    auto descriptor_set_layout = ctx.get_device().get().createDescriptorSetLayout(descriptor_set_layout_create_info);

    vk::PushConstantRange push_constant_range{
        .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR,
        .offset = 0,
        .size = sizeof(SceneAddresses)
    };

    // Ray Tracing Pipeline

    auto rt_pipeline = PipelineBuilder()
                       .rgen("../src/shaders/spirv/raytrace.rgen.spv")
                       .rmiss("../src/shaders/spirv/raytrace.rmiss.spv")
                       .rchit("../src/shaders/spirv/raytrace.rchit.spv")
                       .ray_depth(1)
                       .descriptor_set_layout(descriptor_set_layout)
                       .push_constant_range(push_constant_range)
                       .build(ctx.get_device());

    // Shader Binding Table

    uint32_t handle_size = rt_pipeline_props.shaderGroupHandleSize;
    uint32_t handle_alignment = rt_pipeline_props.shaderGroupHandleAlignment;
    uint32_t base_alignment = rt_pipeline_props.shaderGroupBaseAlignment;
    uint32_t handle_count = rt_pipeline.get_group_count();

    // TODO: to function
    //////////////////////////////////////////////////
    size_t data_size = handle_size * handle_count;

    std::vector<uint8_t> shader_handles = rt_pipeline.get().getRayTracingShaderGroupHandlesKHR<uint8_t>(
        0,
        handle_count,
        data_size);

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
                      .build(ctx.get_allocator());

    {
        auto sbt_address = sbt_buffer.get_device_address(ctx.get_device());

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

    encoder = std::make_unique<Encoder>(ctx.get_device(), frames_in_flight);
    frame_mgr = std::make_unique<FrameManager>(ctx, frames_in_flight, swapchain->get_images().size());

    ctx.get_device().get_queue().waitIdle();

    res = std::make_unique<Resources>(Resources{
        .surface = std::move(surface),
        .sbt_buffer = std::move(sbt_buffer),
        .rgen_region = rgen_region,
        .rmiss_region = rmiss_region,
        .rchit_region = rchit_region,
        .descriptor_set_layout = std::move(descriptor_set_layout),
        .rt_pipeline = std::move(rt_pipeline),
        .rt_image = std::move(rt_image),
        .rt_image_view = std::move(rt_image_view),
    });
}

void Renderer::draw_frame(const Scene& scene) {
    (void) ctx.get_device().get().waitForFences({frame_mgr->get_in_flight_fence()},
                                                vk::True,
                                                std::numeric_limits<uint64_t>::max());

    vk::AcquireNextImageInfoKHR acquire_info{
        .swapchain = swapchain->get(),
        .timeout = std::numeric_limits<uint64_t>::max(),
        .semaphore = frame_mgr->get_image_available_semaphore(),
        .deviceMask = 1,
    };
    auto acquire_result = ctx.get_device().get().acquireNextImage2KHR(acquire_info);

    if (acquire_result.result == vk::Result::eErrorOutOfDateKHR ||
        acquire_result.result == vk::Result::eSuboptimalKHR) {
        recreate();
        frame_mgr->recreate_image_available_semaphores(ctx.get_device());
        return;
    }

    ctx.get_device().get().resetFences({frame_mgr->get_in_flight_fence()});

    uint32_t image_index = acquire_result.value;

    encoder->begin(frame_mgr->get_frame_index());
    auto& cmd = encoder->get_cmd();

    vk::WriteDescriptorSetAccelerationStructureKHR write_as_info{
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &*scene.get_tlas().get_handle(),
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

    auto camera = scene.get_camera();
    auto uniform = Uniform{
        .inv_view = glm::inverse(camera.get_view()),
        .inv_proj = glm::inverse(camera.get_proj()),
    };
    auto& uniform_buffer = frame_mgr->get_uniform_buffer();
    memcpy(uniform_buffer.mapped_ptr(), &uniform, sizeof(Uniform));

    vk::DescriptorBufferInfo descriptor_uniform_info{
        .buffer = uniform_buffer.get(),
        .offset = 0,
        .range = sizeof(Uniform),
    };

    vk::WriteDescriptorSet write_uniform{
        .dstBinding = 2,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .pBufferInfo = &descriptor_uniform_info,
    };

    // std::vector<vk::DescriptorImageInfo> image_infos(scene.get_images().size());
    // for (size_t i = 0; i < scene.get_images().size(); ++i) {
    //     image_infos[i] = {
    //         .sampler = linear_sampler,
    //         .imageView = gpu_textures[i].get_view(),
    //         .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    //     };
    // }

    // vk::WriteDescriptorSet write_images{
    //     .dstBinding = 3,
    //     .descriptorCount = static_cast<uint32_t>(scene.get_images().size()),
    //     .descriptorType = vk::DescriptorType::eCombinedImageSampler,
    //     .pImageInfo = image_infos.data(),
    // };

    std::vector writes{write_as, write_image, write_uniform};

    vk::PushConstantsInfo push_constants_info{
        .layout = res->rt_pipeline.get_layout(),
        .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR,
        .offset = 0,
        .size = sizeof(SceneAddresses),
        .pValues = &scene.get_scene_address(),
    };

    cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, res->rt_pipeline.get());
    cmd.pushDescriptorSet(vk::PipelineBindPoint::eRayTracingKHR, res->rt_pipeline.get_layout(), 0, writes);
    cmd.pushConstants2(push_constants_info);

    cmd.traceRaysKHR(res->rgen_region,
                     res->rmiss_region,
                     res->rchit_region,
                     {},
                     swapchain->get_extent().width,
                     swapchain->get_extent().height,
                     1);

    res->rt_image.transition_layout(cmd,
                                    vk::ImageLayout::eTransferSrcOptimal,
                                    vk::PipelineStageFlagBits2::eTransfer,
                                    vk::AccessFlagBits2::eTransferRead);

    swapchain->get_images()[image_index].transition_layout(cmd,
                                                           vk::ImageLayout::eTransferDstOptimal,
                                                           vk::PipelineStageFlagBits2::eTransfer,
                                                           vk::AccessFlagBits2::eTransferWrite);

    vk::ImageCopy copy_region{
        .srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .extent = vk::Extent3D{swapchain->get_extent().width,
                               swapchain->get_extent().height, 1},
    };

    cmd.copyImage(res->rt_image.get(),
                  vk::ImageLayout::eTransferSrcOptimal,
                  swapchain->get_images()[image_index].get(),
                  vk::ImageLayout::eTransferDstOptimal,
                  copy_region);

    swapchain->get_images()[image_index].transition_layout(cmd,
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

    ctx.get_device().get_queue().submit(submit_info, frame_mgr->get_in_flight_fence());

    vk::PresentInfoKHR present_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*frame_mgr->get_render_finished_semaphore(image_index),
        .swapchainCount = 1,
        .pSwapchains = &*swapchain->get(),
        .pImageIndices = &image_index,
    };

    if (auto result = ctx.get_device().get_queue().presentKHR(present_info);
        result == vk::Result::eSuboptimalKHR ||
        result == vk::Result::eErrorOutOfDateKHR
    ) {
        recreate();
    }
    frame_mgr->update();
}

void Renderer::recreate() {
    SCOPED_TIMER();

    ctx.get_device().get().waitIdle();

    swapchain = std::make_unique<Swapchain>(ctx.get_adapter(), ctx.get_device(), Window::get(), res->surface);

    res->rt_image = ImageBuilder()
                    .type(vk::ImageType::e2D)
                    .format(srgb_to_unorm(swapchain->get_surface_format().format))
                    .size(swapchain->get_extent().width, swapchain->get_extent().height)
                    .mip_levels(1)
                    .layers(1)
                    .samples(vk::SampleCountFlagBits::e1)
                    .usage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc)
                    .build(ctx.get_allocator());

    const auto single_time_encoder = SingleTimeEncoder(ctx.get_device());

    res->rt_image.transition_layout(single_time_encoder.get_cmd(),
                                    vk::ImageLayout::eGeneral,
                                    vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                                    vk::AccessFlagBits2::eShaderWrite);

    single_time_encoder.submit(ctx.get_device());

    const vk::ImageViewCreateInfo rt_image_view_create_info{
        .image = res->rt_image.get(),
        .viewType = vk::ImageViewType::e2D,
        .format = srgb_to_unorm(swapchain->get_surface_format().format),
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    };
    res->rt_image_view = ctx.get_device().get().createImageView(rt_image_view_create_info);
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