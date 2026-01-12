#include "input.h"

#include "vulkan/utils.h"

void Input::init(GLFWwindow* window) {
    SCOPED_TIMER();

    window_handle = window;

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
}

void Input::update() {
    mouse_delta = {0.0f, 0.0f};
    scroll = 0.0;

    std::ranges::copy(keys_current, keys_last.begin());
    std::ranges::copy(mouse_current, mouse_last.begin());

    glfwPollEvents();
}

bool Input::key_down(const int key) {
    return keys_current[key];
}

bool Input::key_pressed(const int key) {
    return keys_current[key] && !keys_last[key];
}

bool Input::key_released(const int key) {
    return !keys_current[key] && keys_last[key];
}

bool Input::mouse_button_down(const int button) {
    return mouse_current[button];
}

bool Input::mouse_button_pressed(const int button) {
    return mouse_current[button] && !mouse_last[button];
}

bool Input::mouse_button_released(const int button) {
    return !mouse_current[button] && mouse_last[button];
}

glm::vec2 Input::get_mouse_pos() {
    return mouse_pos;
}

glm::vec2 Input::get_mouse_delta() {
    return mouse_delta;
}

double Input::get_mouse_scroll() {
    return scroll;
}

void Input::set_cursor_grab(const bool grab) {
    glfwSetInputMode(window_handle, GLFW_CURSOR, grab ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void Input::key_callback(GLFWwindow* /*window*/, const int key, int /*scancode*/, const int action, int /*mods*/) {
    if (action == GLFW_PRESS) {
        keys_current[key] = true;
    } else if (action == GLFW_RELEASE) {
        keys_current[key] = false;
    }
}

void Input::mouse_button_callback(GLFWwindow* /*window*/, const int button, const int action, int /*mods*/) {
    if (action == GLFW_PRESS) {
        mouse_current[button] = true;
    } else if (action == GLFW_RELEASE) {
        mouse_current[button] = false;
    }
}

void Input::cursor_position_callback(GLFWwindow* /*window*/, const double xpos, const double ypos) {
    const glm::vec2 new_pos = {
        static_cast<float>(xpos),
        static_cast<float>(ypos),
    };

    mouse_delta += new_pos - mouse_pos;
    mouse_pos = new_pos;
}

void Input::scroll_callback(GLFWwindow* /*window*/, double /*xoffset*/, const double yoffset) {
    scroll += yoffset;
}