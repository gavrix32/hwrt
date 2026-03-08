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

    vk::raii::DescriptorSetLayout descriptor_set_layout;

    Pipeline rt_pipeline;
    ShaderBindingTable sbt;

    Image rt_image;
    vk::raii::ImageView rt_image_view;

    RenderSettings render_settings;
    Buffer render_settings_buffer;
};

class Renderer {
    Context& ctx;

    std::unique_ptr<Resources> res;
    std::unique_ptr<Encoder> encoder;
    std::unique_ptr<FrameManager> frame_mgr;
    std::unique_ptr<Swapchain> swapchain;

public:
    explicit Renderer(Context& ctx_);

    void draw_frame(const Scene& scene);
    void recreate();
    void update_settings() const;

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
};