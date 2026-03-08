#include "renderer.h"

#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include "vulkan/pipeline.h"
#include "vulkan/utils.h"
#include "camera.h"
#include "gui.h"
#include "window.h"
#include "vulkan/sbt.h"

Renderer::Renderer(Context& ctx_) : ctx(ctx_) {
    SCOPED_TIMER();

    VkSurfaceKHR surface_;
    if (glfwCreateWindowSurface(*ctx.get_instance().get(), Window::get(), nullptr, &surface_) != 0) {
        throw std::runtime_error("Failed to create window surface");
    }
    auto surface = vk::raii::SurfaceKHR(ctx.get_instance().get(), surface_);

    swapchain = std::make_unique<Swapchain>(ctx.get_adapter(), ctx.get_device(), Window::get(), surface);

    auto single_time_encoder = SingleTimeEncoder(ctx.get_device());

    // Ray Trace Image

    auto rt_image = ImageBuilder()
                    .type(vk::ImageType::e2D)
                    .format(swapchain->get_format())
                    .size(swapchain->get_extent().width, swapchain->get_extent().height)
                    .mip_levels(1)
                    .layers(1)
                    .samples(vk::SampleCountFlagBits::e1)
                    .usage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc)
                    .build(ctx.get_allocator());

    vk::ImageViewCreateInfo rt_image_view_create_info{
        .image = rt_image.get(),
        .viewType = vk::ImageViewType::e2D,
        .format = swapchain->get_format(),
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    };
    auto rt_image_view = ctx.get_device().get().createImageView(rt_image_view_create_info);

    rt_image.transition_layout(single_time_encoder.get_cmd(),
                               vk::ImageLayout::eGeneral,
                               vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                               vk::AccessFlagBits2::eShaderWrite);

    single_time_encoder.submit(ctx.get_device());

    // Push Descriptors

    std::vector<vk::DescriptorSetLayoutBinding> push_bindings;

    vk::DescriptorSetLayoutBinding as_binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    };
    push_bindings.push_back(as_binding);

    vk::DescriptorSetLayoutBinding rt_image_binding{
        .binding = 1,
        .descriptorType = vk::DescriptorType::eStorageImage,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    };
    push_bindings.push_back(rt_image_binding);

    vk::DescriptorSetLayoutBinding uniform_binding{
        .binding = 2,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    };
    push_bindings.push_back(uniform_binding);

    // Descriptor set layouts

    vk::DescriptorSetLayoutCreateInfo push_descriptor_set_layout_create_info{
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptor,
        .bindingCount = static_cast<uint32_t>(push_bindings.size()),
        .pBindings = push_bindings.data(),
    };
    auto push_descriptor_set_layout = ctx.get_device().get().createDescriptorSetLayout(push_descriptor_set_layout_create_info);

    // Push constant

    vk::PushConstantRange push_constant_range{
        .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR,
        .offset = 0,
        .size = sizeof(PushData)
    };

    // Ray Tracing Pipeline

    auto rt_pipeline = PipelineBuilder()
                       .rgen_group("../src/shaders/spirv/raytrace.rgen.spv")
                       .rmiss_group("../src/shaders/spirv/raytrace.rmiss.spv")
                       .hit_group("../src/shaders/spirv/raytrace.rchit.spv", std::nullopt)
                       .hit_group("../src/shaders/spirv/raytrace.rchit.spv", "../src/shaders/spirv/raytrace.rahit.spv")
                       .ray_depth(1)
                       .descriptor_set_layout(push_descriptor_set_layout)
                       .descriptor_set_layout(ctx.get_bindless_layout())
                       .push_constant_range(push_constant_range)
                       .build(ctx.get_device());

    // Shader Binding Table

    ShaderBindingTable sbt(ctx.get_adapter(), ctx.get_device(), rt_pipeline, ctx.get_allocator());

    constexpr int frames_in_flight = 2;

    encoder = std::make_unique<Encoder>(ctx.get_device(), frames_in_flight);
    frame_mgr = std::make_unique<FrameManager>(ctx, frames_in_flight, swapchain->get_images().size());

    RenderSettings render_settings{
        .debug_channel = DebugChannel::None
    };

    auto render_settings_buffer = BufferBuilder()
                                  .size(sizeof(RenderSettings))
                                  .usage(vk::BufferUsageFlagBits::eStorageBuffer |
                                         vk::BufferUsageFlagBits::eShaderDeviceAddress)
                                  .allocation_flags(
                                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                                  .build(ctx.get_allocator());

    memcpy(render_settings_buffer.mapped_ptr(), &render_settings, sizeof(render_settings));

    ctx.get_device().get_queue().waitIdle();

    res = std::make_unique<Resources>(Resources{
        .surface = std::move(surface),
        .descriptor_set_layout = std::move(push_descriptor_set_layout),
        .rt_pipeline = std::move(rt_pipeline),
        .sbt = std::move(sbt),
        .rt_image = std::move(rt_image),
        .rt_image_view = std::move(rt_image_view),
        .render_settings = render_settings,
        .render_settings_buffer = std::move(render_settings_buffer),
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

    std::vector writes{write_as, write_image, write_uniform};

    PushData push_data{
        .scene_ptrs = scene.get_scene_ptrs(),
        .render_settings = res->render_settings_buffer.get_device_address(ctx.get_device()),
    };

    vk::PushConstantsInfo push_constants_info{
        .layout = res->rt_pipeline.get_layout(),
        .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR,
        .offset = 0,
        .size = sizeof(PushData),
        .pValues = &push_data,
    };

    vk::BindDescriptorSetsInfo bind_sets_info{
        .stageFlags = vk::ShaderStageFlagBits::eAll,
        .layout = res->rt_pipeline.get_layout(),
        .firstSet = 1,
        .descriptorSetCount = 1,
        .pDescriptorSets = &*scene.get_descriptor_set()
    };

    cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, res->rt_pipeline.get());
    cmd.pushDescriptorSet(vk::PipelineBindPoint::eRayTracingKHR, res->rt_pipeline.get_layout(), 0, writes);
    cmd.bindDescriptorSets2(bind_sets_info);
    cmd.pushConstants2(push_constants_info);

    cmd.traceRaysKHR(res->sbt.get_rgen_region(),
                     res->sbt.get_rmiss_region(),
                     res->sbt.get_hit_region(),
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

    // --- ImGui ---

    swapchain->get_images()[image_index].transition_layout(cmd,
                                                           vk::ImageLayout::eColorAttachmentOptimal,
                                                           vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                           vk::AccessFlagBits2::eColorAttachmentRead |
                                                           vk::AccessFlagBits2::eColorAttachmentWrite);

    vk::RenderingAttachmentInfo attachment_info{
        .imageView = swapchain->get_image_views()[image_index].get(),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
    };

    vk::RenderingInfo rendering_info{
        .renderArea = {{0, 0}, swapchain->get_extent()},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment_info,
    };

    cmd.beginRendering(rendering_info);

    Gui::draw(cmd);

    cmd.endRendering();

    // -------------

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

    Gui::set_image_count(swapchain->get_image_count());

    res->rt_image = ImageBuilder()
                    .type(vk::ImageType::e2D)
                    .format(swapchain->get_format())
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
        .format = swapchain->get_format(),
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    };
    res->rt_image_view = ctx.get_device().get().createImageView(rt_image_view_create_info);
}

void Renderer::update_settings() const {
    memcpy(res->render_settings_buffer.mapped_ptr(), &res->render_settings, sizeof(RenderSettings));
}