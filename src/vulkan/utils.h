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

#ifdef _WIN32
    #define POPEN _popen
    #define PCLOSE _pclose
#else
    #define POPEN popen
    #define PCLOSE pclose
#endif

namespace utils {
    inline void run_bash_script(const std::string& command) {
        const std::string full_command = command + " 2>&1";

        FILE* pipe = POPEN(full_command.c_str(), "r");
        if (!pipe) {
            spdlog::error("Failed to open pipe for command: {}", command);
            return;
        }

        char buffer[1024];
        std::vector<std::string> output_lines;

        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            if (!line.empty() && line.back() == '\n') line.pop_back();
            if (!line.empty() && line.back() == '\r') line.pop_back();

            spdlog::info(line);
            output_lines.push_back(line);
        }

        if (int return_code = PCLOSE(pipe); return_code != 0) {
            spdlog::error("Bash script failed with exit code: {}", return_code);
            if (!output_lines.empty()) {
                spdlog::error("Last message: {}", output_lines.back());
            }
        }
    }
}