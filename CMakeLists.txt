cmake_minimum_required(VERSION 4.0)
project(hwrt)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_subdirectory(external/spdlog)
add_subdirectory(external/vma)
add_subdirectory(external/glm)

#if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#    add_compile_options(-fsanitize=address,undefined -g -fno-omit-frame-pointer)
#    add_link_options(-fsanitize=address,undefined)
#endif()
#
#if(MSVC)
#    add_compile_options(/fsanitize=address)
#endif()

add_executable(hwrt
        src/main.cpp
        src/vulkan/instance.cpp
        src/vulkan/adapter.cpp
        src/vulkan/device.cpp
        src/vulkan/swapchain.cpp
        src/vulkan/allocator.cpp
        src/vulkan/buffer.cpp
        src/vulkan/image.cpp
        src/vulkan/encoder.cpp
        src/context.cpp
        src/frame.cpp
        src/input.cpp
        src/window.cpp
        src/renderer.cpp
)

target_link_libraries(hwrt PRIVATE
        Vulkan::Vulkan
        glfw
        spdlog::spdlog_header_only
        GPUOpen::VulkanMemoryAllocator
        glm::glm
)

target_compile_definitions(hwrt PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
        GLM_ENABLE_EXPERIMENTAL
)