#include "input.h"

void Input::init(GLFWwindow* window) {
    window_handle = window;

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
}

void Input::update() {
    mouse_delta = {0.0f, 0.0f};

    std::copy(keys_current.begin(), keys_current.end(), keys_last.begin());
    std::copy(mouse_current.begin(), mouse_current.end(), mouse_last.begin());

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

glm::vec2 Input::get_mouse_delta() {
    return mouse_delta;
}

glm::vec2 Input::get_mouse_pos() {
    return mouse_pos;
}

void Input::set_cursor_grab(const bool grab) {
    glfwSetInputMode(window_handle, GLFW_CURSOR, grab ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (grab) {
        first_mouse_input = true;
    }
}

void Input::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        keys_current[key] = true;
    } else if (action == GLFW_RELEASE) {
        keys_current[key] = false;
    }
}

void Input::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        mouse_current[button] = true;
    } else if (action == GLFW_RELEASE) {
        mouse_current[button] = false;
    }
}

void Input::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (first_mouse_input) {
        mouse_pos.x = static_cast<float>(xpos);
        mouse_pos.y = static_cast<float>(ypos);
        first_mouse_input = false;
    }

    const glm::vec2 new_pos = {static_cast<float>(xpos), static_cast<float>(ypos)};

    mouse_delta.x = new_pos.x - mouse_pos.x;
    mouse_delta.y = new_pos.y - mouse_pos.y;

    mouse_pos = new_pos;
}