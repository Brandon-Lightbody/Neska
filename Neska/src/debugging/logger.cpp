#include "logger.h"

#include <iostream>
#include <fstream>

Logger::Logger() :
	consoleLoggingEnabled(false),
	fileLoggingEnabled(false)
{
	std::memset(&writeBuffer, 0, sizeof(writeBuffer));
}

void Logger::toggleLogging(bool consoleLogging, bool fileLogging) {
	consoleLoggingEnabled = consoleLogging;
	fileLoggingEnabled = fileLogging;
}

void Logger::logToConsole(std::string debugOutput) {
    logBuffer.push({ debugOutput });
}

void Logger::logToFile(std::string debugOutput) {
    logBuffer.push({ debugOutput });
}

void Logger::handleLogRequests() {
    // if neither target is enabled, nothing to do
    if (!consoleLoggingEnabled && !fileLoggingEnabled) return;

    // open the file once if needed
    std::ofstream fout;
    if (fileLoggingEnabled) {
        // pick a default file name or track it per-Log
        fout.open("log.txt", std::ios::app);
    }

    size_t bufUsed = 0;
    auto flush = [&]() {
        if (bufUsed == 0) return;
        if (consoleLoggingEnabled)
            std::cout.write(writeBuffer.data(), bufUsed);
        if (fileLoggingEnabled)
            fout.write(writeBuffer.data(), bufUsed);
        bufUsed = 0;
        };

    while (!logBuffer.empty()) {
        Log entry = std::move(logBuffer.front());
        const char* txt = entry.debugOutput.c_str();
        size_t      len = std::strlen(txt);

        // if it won’t fit, flush first
        if (len > writeBuffer.size() - bufUsed) flush();

        // copy into your 4 KiB buffer
        std::memcpy(writeBuffer.data() + bufUsed, txt, len);
        bufUsed += len;

        // now *pop* it so it doesn’t sit in memory forever
        logBuffer.pop();
    }

    // final flush of whatever’s left
    flush();
    if (fout.is_open()) fout.close();
}
