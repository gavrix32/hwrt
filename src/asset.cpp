#include "asset.h"
#include "model.h"
#include "fastgltf/base64.hpp"
#include "fastgltf/core.hpp"
#include "vulkan/utils.h"

std::shared_ptr<Model> AssetManager::get_model(const std::filesystem::path& path) {
    if (const auto it = models_cache.find(path); it != models_cache.end()) {
        spdlog::info("AssetManager: Loading from cache: {}", path.string());
        return it->second;
    }

    spdlog::info("AssetManager: Loading from disk: {}", path.string());

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

    models_cache[path] = std::make_shared<Model>(asset);
    return models_cache[path];
}