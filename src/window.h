#pragma once

#include <string>

struct GLFWwindow;

class Window {
    inline static GLFWwindow* handle;
    inline static int width_;
    inline static int height_;
    inline static bool resized;

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
