#pragma once

#include <vulkan/vulkan_raii.hpp>

class Device;

struct RayTracingInfo {
    uint32_t stage_count;
    uint32_t group_count;
    uint32_t rgen_count;
    uint32_t rmiss_count;
    uint32_t hit_count;
};

class RayTracingPipeline {
    vk::raii::Pipeline handle = nullptr;
    vk::raii::PipelineLayout layout = nullptr;
    RayTracingInfo rt_info_{};

public:
    explicit RayTracingPipeline(const std::vector<vk::PipelineShaderStageCreateInfo>& stage_infos,
                                const std::vector<vk::RayTracingShaderGroupCreateInfoKHR>& group_infos,
                                const vk::PipelineLayoutCreateInfo& layout_info,
                                const RayTracingInfo& rt_info,
                                const Device& device);

    ~RayTracingPipeline() = default;

    // Move only
    RayTracingPipeline(const RayTracingPipeline&) = delete;
    RayTracingPipeline& operator=(const RayTracingPipeline&) = delete;
    RayTracingPipeline(RayTracingPipeline&& other) noexcept = default;
    RayTracingPipeline& operator=(RayTracingPipeline&& other) noexcept = default;

    [[nodiscard]] const vk::raii::Pipeline& get() const {
        return handle;
    }

    [[nodiscard]] const vk::raii::PipelineLayout& get_layout() const {
        return layout;
    }

    [[nodiscard]] uint32_t get_group_count() const {
        return rt_info_.group_count;
    }

    [[nodiscard]] uint32_t get_stage_count() const {
        return rt_info_.stage_count;
    }

    [[nodiscard]] uint32_t get_rgen_count() const {
        return rt_info_.rgen_count;
    }

    [[nodiscard]] uint32_t get_rmiss_count() const {
        return rt_info_.rmiss_count;
    }

    [[nodiscard]] uint32_t get_hit_count() const {
        return rt_info_.hit_count;
    }
};

class RayTracingPipelineBuilder {
    struct PendingStage {
        std::string path;
        vk::ShaderStageFlagBits stage;
    };

    struct PendingGroup {
        vk::RayTracingShaderGroupTypeKHR type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
        uint32_t general_idx = vk::ShaderUnusedKHR;
        uint32_t closest_hit_idx = vk::ShaderUnusedKHR;
        uint32_t any_hit_idx = vk::ShaderUnusedKHR;
    };

    std::vector<PendingStage> pending_stages;
    std::vector<PendingGroup> pending_groups;

    uint32_t rgen_count = 0;
    uint32_t rmiss_count = 0;
    uint32_t hit_count = 0;

    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts;
    std::vector<vk::PushConstantRange> push_constant_ranges;

    uint32_t add_stage(const std::string& path, vk::ShaderStageFlagBits stage);

public:
    explicit RayTracingPipelineBuilder();

    RayTracingPipelineBuilder& rgen_group(const std::string& path);
    RayTracingPipelineBuilder& rmiss_group(const std::string& path);
    RayTracingPipelineBuilder& hit_group(const std::optional<std::string>& rchit_path,
                                         const std::optional<std::string>& rahit_path);

    RayTracingPipelineBuilder& descriptor_set_layout(const vk::raii::DescriptorSetLayout& layout);
    RayTracingPipelineBuilder& push_constant_range(vk::PushConstantRange range);

    [[nodiscard]] RayTracingPipeline build(const Device& device) const;
};

class ComputePipeline {
    vk::raii::Pipeline handle = nullptr;
    vk::raii::PipelineLayout layout = nullptr;

public:
    explicit ComputePipeline(const vk::PipelineShaderStageCreateInfo& stage_info,
                             const vk::PipelineLayoutCreateInfo& layout_info,
                             const Device& device);

    ~ComputePipeline() = default;

    // Move only
    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;
    ComputePipeline(ComputePipeline&& other) noexcept = default;
    ComputePipeline& operator=(ComputePipeline&& other) noexcept = default;

    [[nodiscard]] const vk::raii::Pipeline& get() const {
        return handle;
    }

    [[nodiscard]] const vk::raii::PipelineLayout& get_layout() const {
        return layout;
    }
};

class ComputePipelineBuilder {
    std::string stage_ = "";

    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts;
    std::vector<vk::PushConstantRange> push_constant_ranges;

public:
    explicit ComputePipelineBuilder();

    ComputePipelineBuilder& stage(const std::string& path);

    ComputePipelineBuilder& descriptor_set_layout(const vk::raii::DescriptorSetLayout& layout);
    ComputePipelineBuilder& push_constant_range(vk::PushConstantRange range);

    [[nodiscard]] ComputePipeline build(const Device& device) const;
};