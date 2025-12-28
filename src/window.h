#pragma once

#include <string>

struct GLFWwindow;

class Window {
    inline static GLFWwindow* handle;
    inline static int width_;
    inline static int height_;

public:
    static void init(int width, int height, const std::string& title, bool resizable = false);

    static void terminate();

    [[nodiscard]] static GLFWwindow* get();
    [[nodiscard]] static bool should_close();
    [[nodiscard]] static int get_width();
    [[nodiscard]] static int get_height();
    [[nodiscard]] static float get_aspect_ratio();
    
    static void poll_events();
    static void close();
};