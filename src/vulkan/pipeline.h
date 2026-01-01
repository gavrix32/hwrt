#pragma once

#include <vulkan/vulkan_raii.hpp>

class Device;

class Pipeline {
    vk::raii::Pipeline handle;
    vk::raii::PipelineLayout layout;
    uint32_t group_count_;
    uint32_t stage_count_;

public:
    explicit Pipeline(const std::vector<vk::PipelineShaderStageCreateInfo>& stage_infos,
                      const std::vector<vk::RayTracingShaderGroupCreateInfoKHR>& group_infos,
                      uint32_t ray_depth,
                      const vk::PipelineLayoutCreateInfo& layout_info,
                      uint32_t group_count,
                      uint32_t stage_count,
                      const Device& device);

    ~Pipeline() = default;

    // Move only
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&& other) noexcept;
    Pipeline& operator=(Pipeline&& other) noexcept;

    [[nodiscard]] const vk::raii::Pipeline& get() const;
    [[nodiscard]] const vk::raii::PipelineLayout& get_layout() const;
    [[nodiscard]] uint32_t get_group_count() const;
    [[nodiscard]] uint32_t get_stage_count() const;
};

class PipelineBuilder {
    std::vector<std::string> shader_filenames;
    std::vector<vk::ShaderStageFlagBits> stages;
    std::vector<vk::RayTracingShaderGroupTypeKHR> groups;
    uint32_t ray_depth_;
    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts;
    std::vector<vk::PushConstantRange> push_constant_ranges;

public:
    explicit PipelineBuilder();

    PipelineBuilder& rgen(const std::string& filename);
    PipelineBuilder& rmiss(const std::string& filename);
    PipelineBuilder& rchit(const std::string& filename);
    PipelineBuilder& ray_depth(uint32_t depth);
    PipelineBuilder& descriptor_set_layout(const vk::raii::DescriptorSetLayout& layout);
    PipelineBuilder& push_constant_range(vk::PushConstantRange range);

    [[nodiscard]] Pipeline build(const Device& device) const;
};