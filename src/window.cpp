#include <spdlog/spdlog.h>

#include <GLFW/glfw3.h>

#include "window.h"
#include "input.h"
#include "vulkan/utils.h"

void Window::init(int width, int height, const std::string& title) {
    SCOPED_TIMER_NAMED("\"{}\" ({}x{})", title, width, height);

    width_ = width;
    height_ = height;
    resized = false;

    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

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

bool Window::was_resized() {
    if (resized) {
        resized = false;
        return true;
    }
    return false;
}

void Window::poll_events() {
    glfwPollEvents();
}

void Window::close() {
    glfwSetWindowShouldClose(handle, GLFW_TRUE);
}

void Window::framebuffer_size_callback(GLFWwindow* /*window*/, const int width, const int height) {
    width_ = width;
    height_ = height;
    resized = true;
}