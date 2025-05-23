#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>  // Colored console output
//#include <spdlog/sinks/basic_file_sink.h>   // Uncomment for file logging

class Logger {
public:
    // Access the global logger instance
    static std::shared_ptr<spdlog::logger>& get();

    // Optional: Initialize with a specific log level or sink
    static void init(spdlog::level::level_enum level = spdlog::level::debug);

private:
    Logger() = default;
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static std::shared_ptr<spdlog::logger> instance;
};
