#pragma once

#include <array>

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

class Input {
    inline static GLFWwindow* window_handle;

    inline static std::array<bool, GLFW_KEY_LAST + 1> keys_current;
    inline static std::array<bool, GLFW_KEY_LAST + 1> keys_last;

    inline static std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> mouse_current;
    inline static std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> mouse_last;

    inline static glm::vec2 mouse_pos;
    inline static glm::vec2 mouse_delta;

    inline static double scroll;

    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

public:
    static void init(GLFWwindow* window);

    static void update();

    [[nodiscard]] static bool key_down(int key);
    [[nodiscard]] static bool key_pressed(int key);
    [[nodiscard]] static bool key_released(int key);

    [[nodiscard]] static bool mouse_button_down(int button);
    [[nodiscard]] static bool mouse_button_pressed(int button);
    [[nodiscard]] static bool mouse_button_released(int button);

    [[nodiscard]] static glm::vec2 get_mouse_pos();
    [[nodiscard]] static glm::vec2 get_mouse_delta();

    [[nodiscard]] static double get_mouse_scroll();

    static void set_cursor_grab(bool grab);
};