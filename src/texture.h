#pragma once

#include <cstdint>

#include "stb_image.h"

class TextureData {
public:
    unsigned char* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;

    uint32_t metadata_flags = 0;
    static constexpr uint32_t FlagPlaceholder = 1 << 0;

    TextureData() = default;

    ~TextureData() {
        free();
    }

    TextureData(const TextureData&) = delete;
    TextureData& operator=(const TextureData&) = delete;

    TextureData(TextureData&& other) noexcept
        : data(other.data), width(other.width), height(other.height), channels(other.channels),
          metadata_flags(other.metadata_flags) {
        other.data = nullptr;
        other.width = 0;
        other.height = 0;
        other.channels = 0;
        other.metadata_flags = 0;
    }

    TextureData& operator=(TextureData&& other) noexcept {
        if (this != &other) {
            free();

            data = other.data;
            width = other.width;
            height = other.height;
            channels = other.channels;
            metadata_flags = other.metadata_flags;

            other.data = nullptr;
            other.width = 0;
            other.height = 0;
            other.channels = 0;
            other.metadata_flags = 0;
        }
        return *this;
    }

private:
    void free() {
        if (data) {
            stbi_image_free(data);
            data = nullptr;
        }
    }
};