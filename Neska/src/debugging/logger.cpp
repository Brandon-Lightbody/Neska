#include "logger.h"

std::shared_ptr<spdlog::logger> Logger::instance = nullptr;

void Logger::init(spdlog::level::level_enum level) {
    if (!instance) {
        // Create a colored console sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        // Or create a file sink instead:
        // auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt", true);

        instance = std::make_shared<spdlog::logger>("neska", console_sink);
        spdlog::register_logger(instance);
        instance->set_level(level);
        instance->set_pattern("[%H:%M:%S] [%^%l%$] %v");
    }
}

std::shared_ptr<spdlog::logger>& Logger::get() {
    if (!instance) {
        init();  // default to debug level and console sink
    }
    return instance;
}
