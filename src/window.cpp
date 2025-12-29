#include "window.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include "input.h"
#include "vulkan/utils.h"

void Window::init(int width, int height, const std::string& title, const bool resizable) {
    SCOPED_TIMER_NAMED("\"{}\" ({}x{})", title, width, height);

    width_ = width;
    height_ = height;

    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

    if (!glfwInit()) {
        spdlog::critical("Failed to initialize GLFW");
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);

    handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

    if (!handle) {
        spdlog::critical("Failed to create GLFW window");
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    Input::init(handle);
}

void Window::terminate() {
    if (handle) {
        glfwDestroyWindow(handle);
        handle = nullptr;
    }
    glfwTerminate();
    spdlog::info("GLFW terminated");
}

GLFWwindow* Window::get() {
    return handle;
}

bool Window::should_close() {
    return glfwWindowShouldClose(handle);
}

int Window::get_width() {
    return width_;
}

int Window::get_height() {
    return height_;
}

float Window::get_aspect_ratio() {
    return static_cast<float>(width_) / static_cast<float>(height_);
}

void Window::poll_events() {
    glfwPollEvents();
}

void Window::close() {
    glfwSetWindowShouldClose(handle, GLFW_TRUE);
}