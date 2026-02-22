cmake_minimum_required(VERSION 4.0)
project(hwrt)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

find_package(Vulkan REQUIRED)

add_subdirectory(external/spdlog)
add_subdirectory(external/vma)
add_subdirectory(external/glm)
add_subdirectory(external/fastgltf)
add_subdirectory(external/glfw)

#if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#    add_compile_options(-fsanitize=address,undefined -g -fno-omit-frame-pointer)
#    add_link_options(-fsanitize=address,undefined)
#endif()
#
#if(MSVC)
#    add_compile_options(/fsanitize=address)
#endif()

add_compile_definitions(
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
        GLFW_INCLUDE_VULKAN
)

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
        src/vulkan/pipeline.cpp
        src/vulkan/acceleration.cpp
        src/vulkan/sampler.cpp
        src/context.cpp
        src/frame.cpp
        src/input.cpp
        src/window.cpp
        src/renderer.cpp
        src/camera.cpp
        src/scene.cpp
        src/asset.cpp
        src/model.cpp
)

# --- ImGui Setup ---
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
)
set(IMGUI_BACKENDS
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_BACKENDS})

target_include_directories(imgui PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

target_link_libraries(imgui PRIVATE Vulkan::Vulkan glfw)

# --- STB ---
target_include_directories(hwrt PRIVATE external/stb)

target_link_libraries(hwrt PRIVATE
        Vulkan::Vulkan
        glfw
        spdlog::spdlog_header_only
        GPUOpen::VulkanMemoryAllocator
        glm::glm
        fastgltf::fastgltf
        imgui
)

target_compile_definitions(hwrt PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
        GLM_ENABLE_EXPERIMENTAL
)