#pragma once

#include "vulkan/buffer.h"
#include "vulkan/image.h"
#include "vulkan/encoder.h"
#include "vulkan/pipeline.h"
#include "vulkan/swapchain.h"
#include "context.h"
#include "frame.h"
#include "scene.h"
#include "vulkan/sbt.h"

struct Resources {
    vk::raii::SurfaceKHR surface;

    vk::raii::DescriptorSetLayout rt_descriptor_set_layout;
    vk::raii::DescriptorSetLayout compute_descriptor_set_layout;

    RayTracingPipeline rt_pipeline;
    ComputePipeline compute_pipeline;

    ShaderBindingTable sbt;

    Image rt_image;
    Image out_image;
    ImageView rt_image_view;
    ImageView out_image_view;

    RenderSettings render_settings;
    Buffer render_settings_buffer;
};

class Renderer {
    Context& ctx;

    std::unique_ptr<Resources> res;
    std::unique_ptr<Encoder> encoder;
    std::unique_ptr<FrameManager> frame_mgr;
    std::unique_ptr<Swapchain> swapchain;

    uint32_t frame_count = 1;

public:
    explicit Renderer(Context& ctx_);

    void draw_frame(const Scene& scene);
    void recreate();
    void update_settings();

    [[nodiscard]] RenderSettings& get_settings() const {
        return res->render_settings;
    }

    [[nodiscard]] Resources& get_res() const {
        return *res;
    }

    [[nodiscard]] Encoder& get_encoder() const {
        return *encoder;
    }

    [[nodiscard]] FrameManager& get_frame_mgr() const {
        return *frame_mgr;
    }

    [[nodiscard]] Swapchain& get_swapchain() const {
        return *swapchain;
    }

    [[nodiscard]] uint32_t get_frame_count() const {
        return frame_count;
    }
};