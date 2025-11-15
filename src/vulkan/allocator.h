#pragma once

class Instance;
class Adapter;
class Device;

class Allocator {
    VmaAllocator vma_allocator;

public:
    Allocator(const Instance& instance, const Adapter& adapter, const Device& device);
    ~Allocator();
    const VmaAllocator& get() const;
};