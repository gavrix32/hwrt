#include "asset.h"
#include "model.h"
#include "fastgltf/base64.hpp"
#include "fastgltf/core.hpp"
#include "vulkan/utils.h"

Model AssetLoader::load_model(const std::filesystem::path& path) {
    spdlog::info("Loading glTF: {}", path.string());

    if (!std::filesystem::exists(path)) {
        spdlog::critical("File not found: {}", path.string());
    }

    auto data_result = fastgltf::GltfDataBuffer::FromPath(path);
    if (data_result.error() != fastgltf::Error::None) {
        spdlog::error("Failed to load file content: {}", fastgltf::getErrorMessage(data_result.error()));
    }

    fastgltf::GltfDataBuffer data = std::move(data_result.get());

    fastgltf::Parser parser;

    constexpr auto gltf_options = fastgltf::Options::LoadExternalBuffers |
                                 fastgltf::Options::DecomposeNodeMatrices;

    auto asset_result = parser.loadGltf(data, path.parent_path(), gltf_options);

    if (asset_result.error() != fastgltf::Error::None) {
        spdlog::error("Failed to parse glTF: {}", fastgltf::getErrorMessage(asset_result.error()));
    }

    const fastgltf::Asset asset = std::move(asset_result.get());

    return Model(asset);
}