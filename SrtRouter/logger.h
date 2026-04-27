#pragma once

#include <memory>
#include <string>
#include "spdlog/spdlog.h"

namespace spdlog {
    class logger;
}

enum LogLevel {
    LogDebug,
    LogInfo,
    LogWarn,
    LogError
};

class Logger {
public:
    static void init(const std::string& logFile,
                     size_t maxSize = 10 * 1024 * 1024,
                     size_t maxFiles = 30,
                     const std::string& logLevel = "info");
	
	static void shutdown();

    static std::shared_ptr<spdlog::logger> getLogger();

    template<typename... Args>
    static void logPrint(LogLevel level, const char* fmt, Args&&... args);

private:
    static std::shared_ptr<spdlog::logger> sLogger;
};

template<typename... Args>
void Logger::logPrint(LogLevel level, const char* fmt, Args&&... args) {
    auto logger = getLogger();
    if (!logger) return;

    switch (level) {
        case LogDebug:
            logger->debug(fmt, std::forward<Args>(args)...);
            break;
        case LogInfo:
            logger->info(fmt, std::forward<Args>(args)...);
            break;
        case LogWarn:
            logger->warn(fmt, std::forward<Args>(args)...);
            break;
        case LogError:
            logger->error(fmt, std::forward<Args>(args)...);
            break;
    }
}
