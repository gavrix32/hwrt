#include <iostream>
#include <fstream>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 1
#include <vulkan/vulkan_raii.hpp>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include "context.h"
#include "input.h"
#include "renderer.h"
#include "window.h"
#include "vulkan/device.h"

#define WIDTH 800
#define HEIGHT 600

// TODO: Abstractions (pipeline, blas, tlas, sbt, imgui)
// TODO: Check first mouse input

#ifdef NDEBUG
constexpr bool validation = false;
#else
constexpr bool validation = true;
#endif

int main() {
#ifdef NDEBUG
    spdlog::set_level(spdlog::level::info);
#else
    spdlog::set_level(spdlog::level::trace);
#endif

    Window::init(WIDTH, HEIGHT, "hwrt");
    {
        Renderer renderer(validation);

        bool mouse_grab = false;
        while (!Window::should_close()) {
            Input::update();

            if (Input::key_released(GLFW_KEY_ESCAPE)) {
                Window::close();
            }

            if (Input::mouse_button_released(GLFW_MOUSE_BUTTON_RIGHT)) {
                mouse_grab = !mouse_grab;
                Input::set_cursor_grab(mouse_grab);
            }

            renderer.draw_frame();

            static auto last_update_time = std::chrono::high_resolution_clock::now();
            static int frame_count = 0;

            frame_count++;

            auto current_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed_time = current_time - last_update_time;

            if (elapsed_time.count() >= 1.0) {
                double fps = static_cast<double>(frame_count) / elapsed_time.count();
                spdlog::info("{:.0f} fps ({:.2f} ms)", fps, 1000.0 / fps);
                frame_count = 0;
                last_update_time = current_time;
            }
        }
        renderer.get_ctx().get_device().get().waitIdle();
    }
    Window::terminate();

    return 0;
}