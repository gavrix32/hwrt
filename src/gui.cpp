#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "gui.h"

#include "context.h"
#include "window.h"
#include "vulkan/swapchain.h"

void Gui::init(const Context& ctx, const Swapchain& swapchain) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForVulkan(Window::get(), true);

    vk::DescriptorPoolSize pool_sizes[] = {
        {
            .type = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE
        },
    };

    vk::DescriptorPoolCreateInfo pool_info{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 0,
        .poolSizeCount = static_cast<uint32_t>(IM_COUNTOF(pool_sizes)),
        .pPoolSizes = pool_sizes,
    };

    for (const auto& pool_size : pool_sizes) {
        pool_info.maxSets += pool_size.descriptorCount;
    }

    descriptor_pool = ctx.get_device().get().createDescriptorPool(pool_info);

    const auto format = swapchain.get_format();

    vk::PipelineRenderingCreateInfo rendering_info{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &format
    };

    const ImGui_ImplVulkan_PipelineInfo pipeline_info{
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .PipelineRenderingCreateInfo = rendering_info
    };

    ImGui_ImplVulkan_InitInfo init_info{
        .Instance = *ctx.get_instance().get(),
        .PhysicalDevice = *ctx.get_adapter().get(),
        .Device = *ctx.get_device().get(),
        .QueueFamily = ctx.get_device().get_queue_family_index(),
        .Queue = *ctx.get_device().get_queue(),
        .DescriptorPool = *descriptor_pool,
        .MinImageCount = 2,
        .ImageCount = swapchain.get_image_count(),
        .PipelineInfoMain = pipeline_info,
        .PipelineInfoForViewports = pipeline_info,
        .UseDynamicRendering = true,
    };
    ImGui_ImplVulkan_Init(&init_info);
}

void Gui::begin() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Gui::end() {
    ImGui::Render();
}

void Gui::draw(const vk::raii::CommandBuffer& cmd) {
    if (ImDrawData* draw_data = ImGui::GetDrawData()) {
        ImGui_ImplVulkan_RenderDrawData(draw_data, *cmd);
    }
}

void Gui::terminate() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    descriptor_pool = nullptr;
}