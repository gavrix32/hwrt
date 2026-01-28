#pragma once

#include <string>

class Model;

class AssetLoader {
public:
    AssetLoader() = default;

    [[nodiscard]] static Model load_model(const std::string& filename);
};