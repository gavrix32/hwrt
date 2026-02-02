#pragma once

#include "stb_image.h"

class TextureData {
public:
    unsigned char* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;

    TextureData() = default;

    ~TextureData() {
        free();
    }

    TextureData(const TextureData&) = delete;
    TextureData& operator=(const TextureData&) = delete;

    TextureData(TextureData&& other) noexcept
    : data(other.data), width(other.width), height(other.height), channels(other.channels){
        other.data = nullptr;
        other.width = 0;
        other.height = 0;
    }

    TextureData& operator=(TextureData&& other) noexcept {
        if (this != &other) {
            free();

            data = other.data;
            width = other.width;
            height = other.height;
            channels = other.channels;

            other.data = nullptr;
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