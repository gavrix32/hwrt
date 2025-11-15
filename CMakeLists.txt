cmake_minimum_required(VERSION 4.0)
project(hwrt)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_subdirectory(external/spdlog)
add_subdirectory(external/vma)

add_executable(hwrt
        src/main.cpp
        src/vulkan/instance.cpp
        src/vulkan/adapter.cpp
        src/vulkan/device.cpp
        src/vulkan/utils.h
)

target_link_libraries(hwrt PRIVATE
        Vulkan::Vulkan
        glfw
        spdlog::spdlog_header_only
        GPUOpen::VulkanMemoryAllocator
)