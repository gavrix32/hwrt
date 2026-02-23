#pragma once

#include <vulkan/vulkan_raii.hpp>

class Swapchain;
class Context;

class Gui {
    inline static vk::raii::DescriptorPool descriptor_pool = nullptr;

public:
    static void init(const Context& ctx, const Swapchain& swapchain);
    static void begin();
    static void end();
    static void draw(const vk::raii::CommandBuffer& cmd);
    static void terminate();
};