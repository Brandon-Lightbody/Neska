#pragma once

#include <queue>
#include <array>
#include <string>

struct Log {
	std::string debugOutput;
};

class Logger {
public:
	Logger();

	void toggleLogging(bool consoleLogging, bool fileLogging);

	// Logs debug output to the console.
	void logToConsole(const char* debugOutput);

	// Logs debug output to the desired file, creating said file should it not exist.
	void logToFile(const char* debugOutput);
	
	// Merges all debug output into as few writes as possible, then handles all writes.
	void handleLogRequests();
private:
	bool consoleLoggingEnabled;
	bool fileLoggingEnabled;

	// Primary buffer where logs are sent from 'logToConsole' and 'logToFile.'
	std::queue<Log>logBuffer;
	
	// Debug output is merged into this buffer for writing.
	std::array<char, 4096>writeBuffer;
};