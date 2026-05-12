#pragma once

#include <string>

class GLFWwindow;

class Window {
    inline static GLFWwindow* handle = nullptr;
    inline static int width_ = 0;
    inline static int height_ = 0;
    inline static bool resized = false;

public:
    static void init(int width, int height, const std::string& title);
    static void terminate();

    [[nodiscard]] static GLFWwindow* get();
    [[nodiscard]] static bool should_close();
    [[nodiscard]] static int get_width();
    [[nodiscard]] static int get_height();
    [[nodiscard]] static float get_aspect_ratio();
    [[nodiscard]] static bool was_resized();

    static void poll_events();
    static void hide();
    static void show();
    static void close();

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
};
