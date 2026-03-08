#pragma once

#include <vulkan/vulkan_raii.hpp>

class Device;

class Pipeline {
    vk::raii::Pipeline handle;
    vk::raii::PipelineLayout layout;
    uint32_t stage_count_ = 0;
    uint32_t group_count_ = 0;
    uint32_t rgen_count_ = 0;
    uint32_t rmiss_count_ = 0;
    uint32_t hit_count_ = 0;

public:
    explicit Pipeline(const std::vector<vk::PipelineShaderStageCreateInfo>& stage_infos,
                      const std::vector<vk::RayTracingShaderGroupCreateInfoKHR>& group_infos,
                      uint32_t ray_depth,
                      const vk::PipelineLayoutCreateInfo& layout_info,
                      uint32_t stage_count,
                      uint32_t group_count,
                      uint32_t rgen_count,
                      uint32_t rmiss_count,
                      uint32_t hit_count,
                      const Device& device);

    ~Pipeline() = default;

    // Move only
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&& other) noexcept;
    Pipeline& operator=(Pipeline&& other) noexcept;

    [[nodiscard]] const vk::raii::Pipeline& get() const {
        return handle;
    }

    [[nodiscard]] const vk::raii::PipelineLayout& get_layout() const {
        return layout;
    }

    [[nodiscard]] uint32_t get_group_count() const {
        return group_count_;
    }

    [[nodiscard]] uint32_t get_stage_count() const {
        return stage_count_;
    }

    [[nodiscard]] uint32_t get_rgen_count() const {
        return rgen_count_;
    }

    [[nodiscard]] uint32_t get_rmiss_count() const {
        return rmiss_count_;
    }

    [[nodiscard]] uint32_t get_hit_count() const {
        return hit_count_;
    }
};

class PipelineBuilder {
    struct PendingStage {
        std::string path;
        vk::ShaderStageFlagBits stage;
    };

    struct PendingGroup {
        vk::RayTracingShaderGroupTypeKHR type;
        uint32_t general_idx = vk::ShaderUnusedKHR;
        uint32_t closest_hit_idx = vk::ShaderUnusedKHR;
        uint32_t any_hit_idx = vk::ShaderUnusedKHR;
    };

    std::vector<PendingStage> pending_stages;
    std::vector<PendingGroup> pending_groups;

    uint32_t rgen_count = 0;
    uint32_t rmiss_count = 0;
    uint32_t hit_count = 0;

    uint32_t ray_depth_ = 1;
    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts;
    std::vector<vk::PushConstantRange> push_constant_ranges;

    uint32_t add_stage(const std::string& path, vk::ShaderStageFlagBits stage);

public:
    explicit PipelineBuilder();

    PipelineBuilder& rgen_group(const std::string& path);
    PipelineBuilder& rmiss_group(const std::string& path);
    PipelineBuilder& hit_group(const std::optional<std::string>& rchit_path, const std::optional<std::string>& rahit_path);
    PipelineBuilder& ray_depth(uint32_t depth);
    PipelineBuilder& descriptor_set_layout(const vk::raii::DescriptorSetLayout& layout);
    PipelineBuilder& push_constant_range(vk::PushConstantRange range);

    [[nodiscard]] Pipeline build(const Device& device) const;
};