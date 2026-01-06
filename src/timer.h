#pragma once

#include <chrono>

class Timer {
    std::chrono::time_point<std::chrono::high_resolution_clock> last;
    float delta = 0.0f;

public:
    explicit Timer() {
        last = std::chrono::high_resolution_clock::now();
    }

    void tick() {
        const auto curr = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<float> diff = curr - last;

        delta = diff.count();
        last = curr;
    }

    float get_delta() const {
        return delta;
    }
};