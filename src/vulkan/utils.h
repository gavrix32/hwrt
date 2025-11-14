#pragma once

#include <chrono>
#include <string>

#include <spdlog/spdlog.h>

#define SCOPED_TIMER() ScopedTimer timer(__func__)
#define SCOPED_TIMER_NAMED(name) ScopedTimer timer(name)

class ScopedTimer {
public:
    explicit ScopedTimer(const std::string& name) {
        this->name = name;
        start_time = std::chrono::high_resolution_clock::now();
    }

    ~ScopedTimer() {
        const auto end_time = std::chrono::high_resolution_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        double ms = duration / 1000.0;

        spdlog::info("{} ({:.3f} ms)", name, ms);
    }

private:
    std::string name;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
};