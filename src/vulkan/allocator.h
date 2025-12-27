#pragma once

#include <vk_mem_alloc.h>

class Instance;
class Adapter;
class Device;

class Allocator {
    VmaAllocator vma_allocator;

public:
    Allocator(const Instance& instance, const Adapter& adapter, const Device& device);
    ~Allocator();
    [[nodiscard]] const VmaAllocator& get() const;
};