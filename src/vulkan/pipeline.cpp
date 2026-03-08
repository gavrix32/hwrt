#include <fstream>

#include "pipeline.h"
#include "utils.h"
#include "device.h"

Pipeline::Pipeline(const std::vector<vk::PipelineShaderStageCreateInfo>& stage_infos,
                   const std::vector<vk::RayTracingShaderGroupCreateInfoKHR>& group_infos,
                   const uint32_t ray_depth,
                   const vk::PipelineLayoutCreateInfo& layout_info,
                   const uint32_t stage_count,
                   const uint32_t group_count,
                   const uint32_t rgen_count,
                   const uint32_t rmiss_count,
                   const uint32_t hit_count,
                   const Device& device)
    : handle(nullptr),
      layout(nullptr),
      stage_count_(stage_count),
      group_count_(group_count),
      rgen_count_(rgen_count),
      rmiss_count_(rmiss_count),
      hit_count_(hit_count) {
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
      stage_count_(std::exchange(other.stage_count_, 0)),
      group_count_(std::exchange(other.group_count_, 0)) {
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    handle = std::exchange(other.handle, nullptr);
    layout = std::exchange(other.layout, nullptr);

    return *this;
}

PipelineBuilder::PipelineBuilder() = default;

uint32_t PipelineBuilder::add_stage(const std::string& path, const vk::ShaderStageFlagBits stage) {
    pending_stages.push_back({path, stage});
    return static_cast<uint32_t>(pending_stages.size() - 1);
}

PipelineBuilder& PipelineBuilder::rgen_group(const std::string& path) {
    rgen_count++;
    pending_groups.push_back({
        .type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
        .general_idx = add_stage(path, vk::ShaderStageFlagBits::eRaygenKHR)
    });
    return *this;
}

PipelineBuilder& PipelineBuilder::rmiss_group(const std::string& path) {
    rmiss_count++;
    pending_groups.push_back({
        .type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
        .general_idx = add_stage(path, vk::ShaderStageFlagBits::eMissKHR)
    });
    return *this;
}

PipelineBuilder& PipelineBuilder::hit_group(const std::optional<std::string>& rchit_path,
                                            const std::optional<std::string>& rahit_path) {
    hit_count++;
    PendingGroup group{};
    group.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;

    if (rchit_path.has_value()) {
        group.closest_hit_idx = add_stage(rchit_path.value(), vk::ShaderStageFlagBits::eClosestHitKHR);
    }
    if (rahit_path.has_value()) {
        group.any_hit_idx = add_stage(rahit_path.value(), vk::ShaderStageFlagBits::eAnyHitKHR);
    }

    pending_groups.push_back(group);
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
    std::vector<vk::PipelineShaderStageCreateInfo> stage_infos;
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> group_infos;

    modules.reserve(pending_stages.size());
    stage_infos.reserve(pending_stages.size());
    group_infos.reserve(pending_groups.size());

    for (const auto& stage : pending_stages) {
        auto code = read_file(stage.path);

        vk::ShaderModuleCreateInfo module_info{
            .codeSize = code.size() * sizeof(char),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
        };
        modules.emplace_back(device.get().createShaderModule(module_info));

        vk::PipelineShaderStageCreateInfo stage_info{
            .stage = stage.stage,
            .module = modules.back(),
            .pName = "main",
        };
        stage_infos.emplace_back(stage_info);
    }

    for (const auto& group : pending_groups) {
        vk::RayTracingShaderGroupCreateInfoKHR group_info{
            .type = group.type,
            .generalShader = group.general_idx,
            .closestHitShader = group.closest_hit_idx,
            .anyHitShader = group.any_hit_idx,
            .intersectionShader = vk::ShaderUnusedKHR,
        };
        group_infos.emplace_back(group_info);
    }

    const vk::PipelineLayoutCreateInfo layout_info{
        .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size()),
        .pPushConstantRanges = push_constant_ranges.data(),
    };

    return Pipeline(
        stage_infos,
        group_infos,
        ray_depth_,
        layout_info,
        stage_infos.size(),
        group_infos.size(),
        rgen_count,
        rmiss_count,
        hit_count,
        device);
}