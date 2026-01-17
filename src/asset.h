#pragma once

#include <string>

#include "context.h"

class Model;

class AssetLoader {
    const Context& ctx_;

public:
    explicit AssetLoader(const Context& ctx);

    [[nodiscard]] Model load_model(const std::string& filename) const;
};