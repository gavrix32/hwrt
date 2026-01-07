#pragma once

#include "vulkan/buffer.h"
#include "vulkan/image.h"
#include "vulkan/encoder.h"
#include "vulkan/pipeline.h"
#include "vulkan/swapchain.h"
#include "context.h"
#include "frame.h"
#include "camera.h"

struct Resources {
    Buffer vertex_buffer;
    Buffer index_buffer;

    Buffer blas_buffer;
    Buffer tlas_buffer;

    vk::raii::AccelerationStructureKHR blas;
    vk::raii::AccelerationStructureKHR tlas;

    Buffer sbt_buffer;

    vk::StridedDeviceAddressRegionKHR rgen_region{};
    vk::StridedDeviceAddressRegionKHR rmiss_region{};
    vk::StridedDeviceAddressRegionKHR rchit_region{};

    vk::raii::DescriptorSetLayout descriptor_set_layout;
    Pipeline rt_pipeline;

    Image rt_image;
    vk::raii::ImageView rt_image_view;
};

class Renderer {
    std::unique_ptr<Context> ctx;
    std::unique_ptr<Resources> res;
    std::unique_ptr<Encoder> encoder;
    std::unique_ptr<FrameManager> frame_mgr;
    std::unique_ptr<Swapchain> swapchain;

public:
    explicit Renderer(bool validation);

    void draw_frame(const Camera& camera);

    [[nodiscard]] Context& get_ctx() const;
    [[nodiscard]] Resources& get_res() const;
    [[nodiscard]] Encoder& get_encoder() const;
    [[nodiscard]] FrameManager& get_frame_mgr() const;
};