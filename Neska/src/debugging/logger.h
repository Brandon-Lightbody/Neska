#pragma once

#include <quill/Logger.h>
#include <quill/Frontend.h>
#include <quill/Backend.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/backend/PatternFormatter.h>
#include <quill/LogMacros.h>
#include <string_view>

void initLogger();
void shutdownLogger();

void logInfo(std::string_view msg);
void logWarn(std::string_view msg);
void logError(std::string_view msg);
void logDebug(std::string_view msg);

bool debugLoggingEnabled();

void enableDebugLogging();
void disableDebugLogging();

quill::Logger* getQuillLogger();
