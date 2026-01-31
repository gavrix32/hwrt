#pragma once

#include <filesystem>

class Model;

class AssetLoader {
public:
    AssetLoader() = default;

    [[nodiscard]] static Model load_model(const std::filesystem::path& path);
};