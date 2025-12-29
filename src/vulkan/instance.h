#pragma once

#include <vulkan/vulkan_raii.hpp>

class Instance {
    vk::raii::Context vk_context;
    vk::raii::Instance vk_instance;
    vk::raii::DebugUtilsMessengerEXT vk_debug_messenger;

public:
    explicit Instance(bool validation);
    [[nodiscard]] const vk::raii::Instance& get() const;
};
