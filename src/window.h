#pragma once

#include <string>

#include <GLFW/glfw3.h>

class GLFWwindow;

class Window {
    inline static GLFWwindow* handle = nullptr;
    inline static int width_ = 0;
    inline static int height_ = 0;
    inline static bool resized = false;

public:
    static void init(int width, int height, const std::string& title);
    static void terminate();

    [[nodiscard]] static GLFWwindow* get() {
        return handle;
    }

    [[nodiscard]] static bool should_close() {
        return glfwWindowShouldClose(handle);
    }

    [[nodiscard]] static int get_width() {
        return width_;
    }

    [[nodiscard]] static int get_height() {
        return height_;
    }

    [[nodiscard]] static float get_aspect_ratio() {
        return static_cast<float>(width_) / static_cast<float>(height_);
    }

    [[nodiscard]] static bool was_resized() {
        if (resized) {
            resized = false;
            return true;
        }
        return false;
    }

    static void poll_events();
    static void hide();
    static void show();
    static void close();

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
};