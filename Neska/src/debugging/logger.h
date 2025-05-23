#pragma once

#include "debugging/logger.h"

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"
#include <string_view>

class Logger {
public:
	Logger();
	~Logger();



	quill::Logger* getPtr();
private:
	quill::Logger* pLogger;
};