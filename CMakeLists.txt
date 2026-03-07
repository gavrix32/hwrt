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
        src/gui.cpp
        src/tangent.cpp
)

# --- STB Setup ---
set(STB_DIR "${CMAKE_SOURCE_DIR}/external/stb")
set(STB_IMPL_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/stb_impl.cpp")

if (NOT EXISTS ${STB_IMPL_SOURCE})
    file(WRITE ${STB_IMPL_SOURCE}
            "#define STB_IMAGE_IMPLEMENTATION\n"
            "#define STB_IMAGE_WRITE_IMPLEMENTATION\n"
            "#include \"stb_image.h\"\n"
            "#include \"stb_image_write.h\"\n"
    )
endif ()

add_library(stb STATIC ${STB_IMPL_SOURCE})
target_include_directories(stb PUBLIC ${STB_DIR})

if (MSVC)
    target_compile_options(stb PRIVATE /wd4100 /wd4996)
else ()
    target_compile_options(stb PRIVATE -Wno-unused-function)
endif ()
# -----------------

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
# -------------------

# --- MikkTSpace Setup ---
set(MIKKTSPACE_DIR "${CMAKE_SOURCE_DIR}/external/mikktspace")
set(MIKKTSPACE_SOURCES
        ${MIKKTSPACE_DIR}/mikktspace.c
        ${MIKKTSPACE_DIR}/mikktspace.h
)

add_library(mikktspace STATIC ${MIKKTSPACE_SOURCES})

target_include_directories(mikktspace PUBLIC ${MIKKTSPACE_DIR})
# ------------------------

target_link_libraries(imgui PRIVATE Vulkan::Vulkan glfw)

target_include_directories(hwrt PRIVATE src/shaders/slang)

target_link_libraries(hwrt PRIVATE
        Vulkan::Vulkan
        glfw
        spdlog::spdlog_header_only
        GPUOpen::VulkanMemoryAllocator
        glm::glm
        fastgltf::fastgltf
        stb
        imgui
        mikktspace
)

target_compile_definitions(hwrt PRIVATE
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
        GLM_ENABLE_EXPERIMENTAL
)