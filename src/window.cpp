#include <spdlog/spdlog.h>

#include "window.h"
#include "input.h"
#include "vulkan/utils.h"

void Window::init(int width, int height, const std::string& title) {
    SCOPED_TIMER_NAMED("\"{}\" ({}x{})", title, width, height);

    width_ = width;
    height_ = height;
    resized = false;

#ifdef __linux__
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

    if (!glfwInit()) {
        spdlog::critical("Failed to initialize GLFW");
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

    if (!handle) {
        spdlog::critical("Failed to create GLFW window");
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetFramebufferSizeCallback(handle, framebuffer_size_callback);

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

void Window::poll_events() {
    glfwPollEvents();
}

void Window::hide() {
    glfwHideWindow(handle);
}

void Window::show() {
    glfwShowWindow(handle);
}

void Window::close() {
    glfwSetWindowShouldClose(handle, GLFW_TRUE);
}

void Window::framebuffer_size_callback(GLFWwindow* /*window*/, const int width, const int height) {
    width_ = width;
    height_ = height;
    resized = true;
}