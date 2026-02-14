#pragma once

#include <filesystem>
#include <unordered_map>

class Model;

class AssetManager {
    std::unordered_map<std::filesystem::path, std::shared_ptr<Model>> models_cache;

public:
    AssetManager() = default;
    std::shared_ptr<Model> get_model(const std::filesystem::path& path);
};