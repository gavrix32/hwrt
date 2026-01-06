#pragma once

#include <chrono>
#include <string>

#include <spdlog/spdlog.h>

#define SCOPED_TIMER() ScopedTimer timer(__PRETTY_FUNCTION__)
#define SCOPED_TIMER_NAMED(...) ScopedTimer timer(fmt::format("{}: {}", __PRETTY_FUNCTION__, fmt::format(__VA_ARGS__)))

class ScopedTimer {
    std::string name;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;

public:
    explicit ScopedTimer(const std::string& name) {
        this->name = name;
        start_time = std::chrono::high_resolution_clock::now();
    }

    ~ScopedTimer() {
        const auto end_time = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double, std::milli> ms = end_time - start_time;

        spdlog::trace("{} ({:.1f} ms)", name, ms.count());
    }
};