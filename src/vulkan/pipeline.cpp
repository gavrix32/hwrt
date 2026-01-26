#include <fstream>

#include "pipeline.h"
#include "utils.h"
#include "device.h"

Pipeline::Pipeline(const std::vector<vk::PipelineShaderStageCreateInfo>& stage_infos,
                   const std::vector<vk::RayTracingShaderGroupCreateInfoKHR>& group_infos,
                   const uint32_t ray_depth,
                   const vk::PipelineLayoutCreateInfo& layout_info,
                   const uint32_t group_count,
                   const uint32_t stage_count,
                   const Device& device)
    : handle(nullptr), layout(nullptr), group_count_(group_count), stage_count_(stage_count) {
    layout = device.get().createPipelineLayout(layout_info);

    const vk::RayTracingPipelineCreateInfoKHR create_info{
        .stageCount = static_cast<uint32_t>(stage_infos.size()),
        .pStages = stage_infos.data(),
        .groupCount = static_cast<uint32_t>(group_infos.size()),
        .pGroups = group_infos.data(),
        .maxPipelineRayRecursionDepth = ray_depth,
        .layout = *layout,
    };
    handle = device.get().createRayTracingPipelineKHR(nullptr, nullptr, create_info);
}

Pipeline::Pipeline(Pipeline&& other) noexcept
    : handle(std::exchange(other.handle, nullptr)),
      layout(std::exchange(other.layout, nullptr)),
      group_count_(std::exchange(other.group_count_, 0)),
      stage_count_(std::exchange(other.stage_count_, 0)) {
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    handle = std::exchange(other.handle, nullptr);
    layout = std::exchange(other.layout, nullptr);

    return *this;
}

const vk::raii::Pipeline& Pipeline::get() const {
    return handle;
}

const vk::raii::PipelineLayout& Pipeline::get_layout() const {
    return layout;
}

uint32_t Pipeline::get_group_count() const {
    return group_count_;
}

uint32_t Pipeline::get_stage_count() const {
    return stage_count_;
}

PipelineBuilder::PipelineBuilder() = default;

PipelineBuilder& PipelineBuilder::rgen(const std::string& filename) {
    shader_filenames.push_back(filename);
    stages.push_back(vk::ShaderStageFlagBits::eRaygenKHR);
    groups.push_back(vk::RayTracingShaderGroupTypeKHR::eGeneral);
    return *this;
}

PipelineBuilder& PipelineBuilder::rmiss(const std::string& filename) {
    shader_filenames.push_back(filename);
    stages.push_back(vk::ShaderStageFlagBits::eMissKHR);
    groups.push_back(vk::RayTracingShaderGroupTypeKHR::eGeneral);
    return *this;
}

PipelineBuilder& PipelineBuilder::rchit(const std::string& filename) {
    shader_filenames.push_back(filename);
    stages.push_back(vk::ShaderStageFlagBits::eClosestHitKHR);
    groups.push_back(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);
    return *this;
}

PipelineBuilder& PipelineBuilder::ray_depth(const uint32_t depth) {
    ray_depth_ = depth;
    return *this;
}

PipelineBuilder& PipelineBuilder::descriptor_set_layout(const vk::raii::DescriptorSetLayout& layout) {
    descriptor_set_layouts.push_back(*layout);
    return *this;
}

PipelineBuilder& PipelineBuilder::push_constant_range(const vk::PushConstantRange range) {
    push_constant_ranges.push_back(range);
    return *this;
}

std::vector<char> read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", filename);
    }

    std::vector<char> buffer(file.tellg());

    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();

    return buffer;
}

Pipeline PipelineBuilder::build(const Device& device) const {
    std::vector<vk::raii::ShaderModule> modules;

    for (const auto& filename : shader_filenames) {
        auto code = read_file(filename);

        vk::ShaderModuleCreateInfo module_info{
            .codeSize = code.size() * sizeof(char),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
        };
        modules.push_back(device.get().createShaderModule(module_info));
    }

    std::vector<vk::PipelineShaderStageCreateInfo> stage_infos;
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> group_infos;

    for (int i = 0; i < shader_filenames.size(); ++i) {
        vk::PipelineShaderStageCreateInfo stage_info{
            .stage = stages[i],
            .module = modules[i],
            .pName = "main",
        };
        stage_infos.push_back(stage_info);

        vk::RayTracingShaderGroupCreateInfoKHR group_info{
            .type = groups[i],
            .generalShader = vk::ShaderUnusedKHR,
            .closestHitShader = vk::ShaderUnusedKHR,
            .anyHitShader = vk::ShaderUnusedKHR,
            .intersectionShader = vk::ShaderUnusedKHR,
        };

        if (groups[i] == vk::RayTracingShaderGroupTypeKHR::eGeneral) {
            group_info.generalShader = i;
        } else if (groups[i] == vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup) {
            group_info.closestHitShader = i;
        }

        group_infos.push_back(group_info);
    }

    const vk::PipelineLayoutCreateInfo layout_info{
        .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size()),
        .pPushConstantRanges = push_constant_ranges.data(),
    };

    return Pipeline(stage_infos, group_infos, ray_depth_, layout_info, groups.size(), stages.size(), device);
}