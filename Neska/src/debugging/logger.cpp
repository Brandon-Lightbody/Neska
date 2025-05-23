#include "logger.h"

#include <iostream>

Logger::Logger() {
    if (!quill::Backend::is_running()) {
        quill::Backend::start();
    }

    pLogger = quill::Frontend::create_or_get_logger(
        "root",
        quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"),
        quill::PatternFormatterOptions{
        "%(time) [%(thread_id)] %(short_source_location:<28) "
        "LOG_%(log_level:<9) %(logger:<12) %(message)",
        "%H:%M:%S.%Qns",
        quill::Timezone::GmtTime });
}

Logger::~Logger() {
    if (quill::Backend::is_running()) {
        quill::Backend::stop();
    }
}

quill::Logger* Logger::getPtr() {
    return pLogger;
}