#include "sampler.h"

Sampler::Sampler(const Device& device, const vk::Filter mag_filter, const vk::Filter min_filter) : handle(nullptr) {
    const vk::SamplerCreateInfo create_info = {
        .magFilter = mag_filter,
        .minFilter = min_filter,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0,
        .anisotropyEnable = vk::False,
        .maxAnisotropy = 1.0f,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = vk::LodClampNone,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = vk::False,
    };
    handle = device.get().createSampler(create_info);
}