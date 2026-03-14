#include <fstream>

#include "pipeline.h"
#include "utils.h"
#include "device.h"

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

RayTracingPipeline::RayTracingPipeline(const std::vector<vk::PipelineShaderStageCreateInfo>& stage_infos,
                                       const std::vector<vk::RayTracingShaderGroupCreateInfoKHR>& group_infos,
                                       const vk::PipelineLayoutCreateInfo& layout_info,
                                       const RayTracingInfo& rt_info,
                                       const Device& device)
    : rt_info_(rt_info) {
    layout = device.get().createPipelineLayout(layout_info);

    const vk::RayTracingPipelineCreateInfoKHR create_info{
        .stageCount = static_cast<uint32_t>(stage_infos.size()),
        .pStages = stage_infos.data(),
        .groupCount = static_cast<uint32_t>(group_infos.size()),
        .pGroups = group_infos.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = *layout,
    };
    handle = device.get().createRayTracingPipelineKHR(nullptr, nullptr, create_info);
}

RayTracingPipelineBuilder::RayTracingPipelineBuilder() = default;

uint32_t RayTracingPipelineBuilder::add_stage(const std::string& path, const vk::ShaderStageFlagBits stage) {
    pending_stages.push_back({path, stage});
    return static_cast<uint32_t>(pending_stages.size() - 1);
}

RayTracingPipelineBuilder& RayTracingPipelineBuilder::rgen_group(const std::string& path) {
    rgen_count++;
    pending_groups.push_back({
        .type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
        .general_idx = add_stage(path, vk::ShaderStageFlagBits::eRaygenKHR)
    });
    return *this;
}

RayTracingPipelineBuilder& RayTracingPipelineBuilder::rmiss_group(const std::string& path) {
    rmiss_count++;
    pending_groups.push_back({
        .type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
        .general_idx = add_stage(path, vk::ShaderStageFlagBits::eMissKHR)
    });
    return *this;
}

RayTracingPipelineBuilder& RayTracingPipelineBuilder::hit_group(const std::optional<std::string>& rchit_path,
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

RayTracingPipelineBuilder& RayTracingPipelineBuilder::descriptor_set_layout(const vk::raii::DescriptorSetLayout& layout) {
    descriptor_set_layouts.push_back(*layout);
    return *this;
}

RayTracingPipelineBuilder& RayTracingPipelineBuilder::push_constant_range(const vk::PushConstantRange range) {
    push_constant_ranges.push_back(range);
    return *this;
}

RayTracingPipeline RayTracingPipelineBuilder::build(const Device& device) const {
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

    const RayTracingInfo rt_info{
        .stage_count = static_cast<uint32_t>(stage_infos.size()),
        .group_count = static_cast<uint32_t>(group_infos.size()),
        .rgen_count = rgen_count,
        .rmiss_count = rmiss_count,
        .hit_count = hit_count,
    };

    return RayTracingPipeline(
        stage_infos,
        group_infos,
        layout_info,
        rt_info,
        device);
}

ComputePipeline::ComputePipeline(const vk::PipelineShaderStageCreateInfo& stage_info,
                                 const vk::PipelineLayoutCreateInfo& layout_info,
                                 const Device& device) {
    layout = device.get().createPipelineLayout(layout_info);

    const vk::ComputePipelineCreateInfo create_info{
        .stage = stage_info,
        .layout = *layout,
    };
    handle = device.get().createComputePipeline(nullptr, create_info);
}

ComputePipelineBuilder::ComputePipelineBuilder() = default;

ComputePipelineBuilder& ComputePipelineBuilder::stage(const std::string& path) {
    stage_ = path;
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::descriptor_set_layout(const vk::raii::DescriptorSetLayout& layout) {
    descriptor_set_layouts.push_back(*layout);
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::push_constant_range(const vk::PushConstantRange range) {
    push_constant_ranges.push_back(range);
    return *this;
}

ComputePipeline ComputePipelineBuilder::build(const Device& device) const {
    const auto code = read_file(stage_);

    const vk::ShaderModuleCreateInfo module_info{
        .codeSize = code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    };
    const auto module = device.get().createShaderModule(module_info);

    const vk::PipelineShaderStageCreateInfo stage_info{
        .stage = vk::ShaderStageFlagBits::eCompute,
        .module = module,
        .pName = "main",
    };

    const vk::PipelineLayoutCreateInfo layout_info{
        .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size()),
        .pPushConstantRanges = push_constant_ranges.data(),
    };

    return ComputePipeline(stage_info, layout_info, device);
}