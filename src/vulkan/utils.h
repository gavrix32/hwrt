#pragma once

#include <chrono>
#include <string>
#include <filesystem>

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

#ifdef __linux__
    #define POPEN popen
    #define PCLOSE pclose
    #include <unistd.h>
#else
    #define POPEN _popen
    #define PCLOSE _pclose
    #include <windows.h>
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

    inline std::filesystem::path get_exec_path() {
        char buffer[1024];
        #if __linux__
            ssize_t count = readlink("/proc/self/exe", buffer, sizeof(buffer));
            if (count != -1) buffer[count] = '\0';
        #else
            GetModuleFileNameA(NULL, buffer, sizeof(buffer));
        #endif

        return {buffer};
    }
}