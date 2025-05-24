#include "logger.h"

static quill::Logger* g_logger = nullptr;

static bool debugLogging = false;

void initLogger() {
    if (!quill::Backend::is_running()) {

        quill::BackendOptions backOptions;
        backOptions.check_backend_singleton_instance = true;
        backOptions.sleep_duration = std::chrono::milliseconds(1);
        backOptions.sink_min_flush_interval = std::chrono::milliseconds(10);
        backOptions.wait_for_queues_to_empty_before_exit = true;

        quill::Backend::start(backOptions);
    }

    auto sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_sink");

    quill::PatternFormatterOptions format{
        "%(time) [%(thread_id)] %(short_source_location:<28) "
        "LOG_%(log_level:<9) %(logger:<12) %(message)",
        "%H:%M:%S.%Qns",
        quill::Timezone::LocalTime
    };

    g_logger = quill::Frontend::create_or_get_logger("logger", sink, format);
}

void shutdownLogger() {
    g_logger->flush_log();
    quill::Frontend::remove_logger(g_logger);
    quill::Backend::stop();
    g_logger = nullptr;
}

void logInfo(std::string_view msg) {
    if (debugLoggingEnabled() && g_logger) {
        LOG_INFO(getQuillLogger(), "{}", msg);
    }
}

void logWarn(std::string_view msg) {
    if (debugLoggingEnabled() && g_logger) {
        LOG_WARNING(getQuillLogger(), "{}", msg);
    }
}

void logError(std::string_view msg) {
    if (debugLoggingEnabled() && g_logger) {
        LOG_ERROR(getQuillLogger(), "{}", msg);
    }
}

void logDebug(std::string_view msg) {
    if (debugLoggingEnabled() && g_logger) {
        LOG_DEBUG(getQuillLogger(), "{}", msg);
    }
}

bool debugLoggingEnabled() {
    return debugLogging;
}

void enableDebugLogging() {
    if (!debugLoggingEnabled()) {
        debugLogging = true;
    }
}

void disableDebugLogging() {
    if (debugLoggingEnabled()) {
        debugLogging = false;
    }
}

quill::Logger* getQuillLogger() {
    if (!g_logger) {
        initLogger();
    }
    return g_logger;
}