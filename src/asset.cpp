#include <tiny_gltf.h>

#include "asset.h"
#include "model.h"
#include "vulkan/utils.h"

AssetLoader::AssetLoader(const Context& ctx) : ctx_(ctx) {
}

Model AssetLoader::load_model(const std::string& filename) const {
    spdlog::info("Loading glTF model...");

    tinygltf::Model gltf_model;
    tinygltf::TinyGLTF gltf_loader;
    std::string err;
    std::string warn;

    const bool ret = gltf_loader.LoadBinaryFromFile(&gltf_model, &err, &warn, filename);

    if (!warn.empty()) spdlog::warn("glTF warning: {}", warn);
    if (!err.empty()) spdlog::error("glTF error: {}", err);
    if (!ret) spdlog::critical("glTF failed to parse: {}", filename);

    return Model(ctx_, gltf_model);
}