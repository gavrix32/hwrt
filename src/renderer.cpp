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

RayTracingPipeline create_rt_pipeline(const Context& ctx, const vk::raii::DescriptorSetLayout& layout) {
    constexpr vk::PushConstantRange rt_push_constant_range{
        .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR |
                      vk::ShaderStageFlagBits::eClosestHitKHR |
                      vk::ShaderStageFlagBits::eAnyHitKHR,
        .offset = 0,
        .size = sizeof(PushData)
    };

    const auto exec_path = utils::get_exec_path();
    const auto build_dir = exec_path.parent_path();
    const auto spirv_dir = build_dir.parent_path() / "src" / "shaders" / "spirv";

    return RayTracingPipelineBuilder()
           .rgen_group((spirv_dir / "raytrace.rgen.spv").string())
           .rmiss_group((spirv_dir / "raytrace.rmiss.spv").string())
           .hit_group((spirv_dir / "raytrace.rchit.spv").string(), std::nullopt)
           .hit_group((spirv_dir / "raytrace.rchit.spv").string(), (spirv_dir / "raytrace.rahit.spv").string())
           .descriptor_set_layout(layout)
           .descriptor_set_layout(ctx.get_bindless_layout())
           .push_constant_range(rt_push_constant_range)
           .build(ctx.get_device());
}

ComputePipeline create_compute_pipeline(const Context& ctx, const vk::raii::DescriptorSetLayout& layout) {
    constexpr vk::PushConstantRange compute_push_constant_range{
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(PushData)
    };

    const auto exec_path = utils::get_exec_path();
    const auto build_dir = exec_path.parent_path();
    const auto spirv_dir = build_dir.parent_path() / "src" / "shaders" / "spirv";

    return ComputePipelineBuilder()
           .stage((spirv_dir / "compute.spv").string())
           .descriptor_set_layout(layout)
           .push_constant_range(compute_push_constant_range)
           .build(ctx.get_device());
}

ShaderBindingTable create_sbt(const Context& ctx, const RayTracingPipeline& rt_pipeline) {
    return ShaderBindingTable(ctx.get_adapter(), ctx.get_device(), rt_pipeline, ctx.get_allocator());
}

Renderer::Renderer(Context& ctx_) : ctx(ctx_) {
    SCOPED_TIMER();

    VkSurfaceKHR surface_;
    if (glfwCreateWindowSurface(*ctx.get_instance().get(), Window::get(), nullptr, &surface_) != 0) {
        throw std::runtime_error("Failed to create window surface");
    }
    auto surface = vk::raii::SurfaceKHR(ctx.get_instance().get(), surface_);

    swapchain = std::make_unique<Swapchain>(ctx.get_adapter(), ctx.get_device(), Window::get(), surface, nullptr);

    auto single_time_encoder = SingleTimeEncoder(ctx.get_device());

    // Ray Trace Image

    auto rt_image = ImageBuilder()
                    .type(vk::ImageType::e2D)
                    .format(vk::Format::eR32G32B32A32Sfloat)
                    .size(swapchain->get_extent().width, swapchain->get_extent().height)
                    .mip_levels(1)
                    .layers(1)
                    .samples(vk::SampleCountFlagBits::e1)
                    .usage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc)
                    .build(ctx.get_allocator());

    auto rt_image_view = ImageView(ctx.get_device(), rt_image, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, 0, 1);

    rt_image.transition_layout(single_time_encoder.get_cmd(),
                               vk::ImageLayout::eGeneral,
                               vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                               vk::AccessFlagBits2::eShaderWrite);

    // Out Image

    auto out_image = ImageBuilder()
                     .type(vk::ImageType::e2D)
                     .format(swapchain->get_format())
                     .size(swapchain->get_extent().width, swapchain->get_extent().height)
                     .mip_levels(1)
                     .layers(1)
                     .samples(vk::SampleCountFlagBits::e1)
                     .usage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc)
                     .build(ctx.get_allocator());

    auto out_image_view = ImageView(ctx.get_device(), out_image, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, 0, 1);

    out_image.transition_layout(single_time_encoder.get_cmd(),
                                vk::ImageLayout::eGeneral,
                                vk::PipelineStageFlagBits2::eComputeShader,
                                vk::AccessFlagBits2::eShaderWrite);

    single_time_encoder.submit(ctx.get_device());

    // Ray Tracing Push Descriptors

    std::vector<vk::DescriptorSetLayoutBinding> rt_push_bindings;

    vk::DescriptorSetLayoutBinding as_binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    };
    rt_push_bindings.push_back(as_binding);

    vk::DescriptorSetLayoutBinding rt_storage_image_binding{
        .binding = 1,
        .descriptorType = vk::DescriptorType::eStorageImage,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    };
    rt_push_bindings.push_back(rt_storage_image_binding);

    vk::DescriptorSetLayoutBinding uniform_binding{
        .binding = 2,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    };
    rt_push_bindings.push_back(uniform_binding);

    // Ray Tracing Descriptor set layouts

    vk::DescriptorSetLayoutCreateInfo rt_push_descriptor_set_layout_info{
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptor,
        .bindingCount = static_cast<uint32_t>(rt_push_bindings.size()),
        .pBindings = rt_push_bindings.data(),
    };
    auto rt_push_descriptor_set_layout = ctx.get_device().get().createDescriptorSetLayout(rt_push_descriptor_set_layout_info);

    // Compute Push Descriptors

    std::vector<vk::DescriptorSetLayoutBinding> compute_push_bindings;

    vk::DescriptorSetLayoutBinding compute_storage_image_binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eStorageImage,
        .descriptorCount = 2,
        .stageFlags = vk::ShaderStageFlagBits::eAll,
    };
    compute_push_bindings.push_back(compute_storage_image_binding);

    // Compute Descriptor set layouts

    vk::DescriptorSetLayoutCreateInfo compute_push_descriptor_set_layout_info{
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptor,
        .bindingCount = static_cast<uint32_t>(compute_push_bindings.size()),
        .pBindings = compute_push_bindings.data(),
    };
    auto compute_push_descriptor_set_layout =
        ctx.get_device().get().createDescriptorSetLayout(compute_push_descriptor_set_layout_info);

    // Ray Tracing Pipeline

    auto rt_pipeline = create_rt_pipeline(ctx, rt_push_descriptor_set_layout);

    // Compute Pipeline

    auto compute_pipeline = create_compute_pipeline(ctx, compute_push_descriptor_set_layout);

    // Shader Binding Table

    auto sbt = create_sbt(ctx, rt_pipeline);

    constexpr int frames_in_flight = 2;

    encoder = std::make_unique<Encoder>(ctx.get_device(), frames_in_flight);
    frame_mgr = std::make_unique<FrameManager>(ctx, frames_in_flight, swapchain->get_images().size());

    RenderSettings render_settings{
        .debug_channel = DebugChannel::None,
        .samples = 1,
        .max_depth = 4,
        .iterations = UINT32_MAX,
        .nee = 1,
        .mis = 0
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
        .rt_descriptor_set_layout = std::move(rt_push_descriptor_set_layout),
        .compute_descriptor_set_layout = std::move(compute_push_descriptor_set_layout),
        .rt_pipeline = std::move(rt_pipeline),
        .compute_pipeline = std::move(compute_pipeline),
        .sbt = std::move(sbt),
        .rt_image = std::move(rt_image),
        .out_image = std::move(out_image),
        .rt_image_view = std::move(rt_image_view),
        .out_image_view = std::move(out_image_view),
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

    // Ray Tracing writes

    vk::WriteDescriptorSetAccelerationStructureKHR write_as_info{
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &*scene.get_tlas().get_handle(),
    };

    vk::WriteDescriptorSet rt_write_as{
        .pNext = &write_as_info,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
    };

    vk::DescriptorImageInfo rt_descriptor_image_info{
        .imageView = res->rt_image_view.get(),
        .imageLayout = vk::ImageLayout::eGeneral,
    };

    vk::WriteDescriptorSet rt_write_image{
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eStorageImage,
        .pImageInfo = &rt_descriptor_image_info,
    };

    auto camera = scene.get_camera();
    auto uniform = Uniform{
        .inv_view = glm::inverse(camera.get_view()),
        .inv_proj = glm::inverse(camera.get_proj()),
    };
    auto& uniform_buffer = frame_mgr->get_uniform_buffer();

    bool frame_reset = false;

    if (std::memcmp(uniform_buffer.mapped_ptr(), &uniform, sizeof(Uniform)) != 0) {
        frame_reset = true;
        frame_count = 1;
        memcpy(uniform_buffer.mapped_ptr(), &uniform, sizeof(Uniform));
    }

    vk::DescriptorBufferInfo descriptor_uniform_info{
        .buffer = uniform_buffer.get(),
        .offset = 0,
        .range = sizeof(Uniform),
    };

    vk::WriteDescriptorSet rt_write_uniform{
        .dstBinding = 2,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .pBufferInfo = &descriptor_uniform_info,
    };

    std::vector rt_writes{rt_write_as, rt_write_image, rt_write_uniform};

    // Compute writes

    vk::DescriptorImageInfo compute_descriptor_rt_image_info{
        .imageView = res->rt_image_view.get(),
        .imageLayout = vk::ImageLayout::eGeneral,
    };

    vk::DescriptorImageInfo compute_descriptor_out_image_info{
        .imageView = res->out_image_view.get(),
        .imageLayout = vk::ImageLayout::eGeneral,
    };

    std::vector compute_descriptor_image_infos{compute_descriptor_rt_image_info, compute_descriptor_out_image_info};

    vk::WriteDescriptorSet compute_write_out_images{
        .dstBinding = 0,
        .descriptorCount = static_cast<uint32_t>(compute_descriptor_image_infos.size()),
        .descriptorType = vk::DescriptorType::eStorageImage,
        .pImageInfo = compute_descriptor_image_infos.data(),
    };

    std::vector compute_writes{compute_write_out_images};

    // Push constants

    PushData push_data{
        .scene_ptrs = scene.get_scene_ptrs(),
        .render_settings = res->render_settings_buffer.get_device_address(ctx.get_device()),
        .frame_count = frame_count,
        .num_lights = scene.get_num_lights()
    };

    vk::PushConstantsInfo rt_push_constants_info{
        .layout = res->rt_pipeline.get_layout(),
        .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR |
                      vk::ShaderStageFlagBits::eClosestHitKHR |
                      vk::ShaderStageFlagBits::eAnyHitKHR,
        .offset = 0,
        .size = sizeof(PushData),
        .pValues = &push_data,
    };

    vk::PushConstantsInfo compute_push_constants_info{
        .layout = res->compute_pipeline.get_layout(),
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(PushData),
        .pValues = &push_data,
    };

    // Ray Tracing Bind Descriptor Sets

    vk::BindDescriptorSetsInfo bind_sets_info{
        .stageFlags = vk::ShaderStageFlagBits::eAll,
        .layout = res->rt_pipeline.get_layout(),
        .firstSet = 1,
        .descriptorSetCount = 1,
        .pDescriptorSets = &*scene.get_descriptor_set()
    };

    if (frame_count <= res->render_settings.iterations) {
        cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, res->rt_pipeline.get());
        cmd.pushDescriptorSet(vk::PipelineBindPoint::eRayTracingKHR, res->rt_pipeline.get_layout(), 0, rt_writes);
        cmd.bindDescriptorSets2(bind_sets_info);
        cmd.pushConstants2(rt_push_constants_info);

        cmd.traceRaysKHR(res->sbt.get_rgen_region(),
                         res->sbt.get_rmiss_region(),
                         res->sbt.get_hit_region(),
                         {},
                         swapchain->get_extent().width,
                         swapchain->get_extent().height,
                         1);

        res->rt_image.transition_layout(cmd,
                                        vk::ImageLayout::eGeneral,
                                        vk::PipelineStageFlagBits2::eComputeShader,
                                        vk::AccessFlagBits2::eShaderStorageRead);

        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, res->compute_pipeline.get());
        cmd.pushDescriptorSet(vk::PipelineBindPoint::eCompute, res->compute_pipeline.get_layout(), 0, compute_writes);
        cmd.pushConstants2(compute_push_constants_info);

        uint32_t group_count_x = (swapchain->get_extent().width + 16 - 1) / 16;
        uint32_t group_count_y = (swapchain->get_extent().height + 16 - 1) / 16;

        cmd.dispatch(group_count_x, group_count_y, 1);
    }

    res->out_image.transition_layout(cmd,
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

    cmd.copyImage(res->out_image.get(),
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
                                    vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite);

    res->out_image.transition_layout(cmd,
                                     vk::ImageLayout::eGeneral,
                                     vk::PipelineStageFlagBits2::eComputeShader,
                                     vk::AccessFlagBits2::eShaderStorageWrite);

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

    if (!frame_reset && frame_count <= res->render_settings.iterations) {
        frame_count++;
    }
}

void Renderer::recreate() {
    SCOPED_TIMER();

    ctx.get_device().get().waitIdle();

    const auto old_swapchain = std::move(swapchain);

    swapchain = std::make_unique<Swapchain>(ctx.get_adapter(), ctx.get_device(), Window::get(), res->surface,
                                            old_swapchain->get());

    Gui::set_image_count(swapchain->get_image_count());

    const auto single_time_encoder = SingleTimeEncoder(ctx.get_device());

    // Ray Tracing Image

    res->rt_image = ImageBuilder()
                    .type(vk::ImageType::e2D)
                    .format(res->rt_image.format_)
                    .size(swapchain->get_extent().width, swapchain->get_extent().height)
                    .mip_levels(1)
                    .layers(1)
                    .samples(vk::SampleCountFlagBits::e1)
                    .usage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc)
                    .build(ctx.get_allocator());

    res->rt_image.transition_layout(single_time_encoder.get_cmd(),
                                    vk::ImageLayout::eGeneral,
                                    vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
                                    vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite);

    // Out Image

    res->out_image = ImageBuilder()
                     .type(vk::ImageType::e2D)
                     .format(res->out_image.format_)
                     .size(swapchain->get_extent().width, swapchain->get_extent().height)
                     .mip_levels(1)
                     .layers(1)
                     .samples(vk::SampleCountFlagBits::e1)
                     .usage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc)
                     .build(ctx.get_allocator());

    res->out_image.transition_layout(single_time_encoder.get_cmd(),
                                     vk::ImageLayout::eGeneral,
                                     vk::PipelineStageFlagBits2::eComputeShader,
                                     vk::AccessFlagBits2::eShaderStorageWrite);

    single_time_encoder.submit(ctx.get_device());

    // Image views

    res->rt_image_view =
        ImageView(ctx.get_device(), res->rt_image, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, 0, 1);

    res->out_image_view =
        ImageView(ctx.get_device(), res->out_image, vk::ImageViewType::e2D, vk::ImageAspectFlagBits::eColor, 0, 1);

    frame_count = 1;
}

void Renderer::update_settings() {
    memcpy(res->render_settings_buffer.mapped_ptr(), &res->render_settings, sizeof(RenderSettings));
    frame_count = 1;
}

void Renderer::reload_shaders() {
    ctx.get_device().get().waitIdle();

    const auto exec_path = utils::get_exec_path();
    const auto build_dir = exec_path.parent_path();
    const auto shader_dir = build_dir.parent_path() / "src" / "shaders";

    utils::run_bash_script("bash " + (shader_dir / "compile.sh").string());
    res->rt_pipeline = create_rt_pipeline(ctx, res->rt_descriptor_set_layout);
    res->compute_pipeline = create_compute_pipeline(ctx, res->compute_descriptor_set_layout);
    res->sbt = create_sbt(ctx, res->rt_pipeline);
    frame_count = 1;
}