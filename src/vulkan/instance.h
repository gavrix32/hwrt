#pragma once

class Instance {
    vk::raii::Context vk_context;
    vk::raii::Instance vk_instance;

public:
    explicit Instance(bool validation);
    const vk::raii::Instance& get() const;
};