#include "logger.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_sinks.h"
#include <vector>
#include <mutex>
#include <algorithm>

std::shared_ptr<spdlog::logger> Logger::sLogger = nullptr;

static spdlog::level::level_enum parseLogLevel(const std::string& levelStr) {
    std::string lower = levelStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "trace") return spdlog::level::trace;
    if (lower == "debug") return spdlog::level::debug;
    if (lower == "info") return spdlog::level::info;
    if (lower == "warn" || lower == "warning") return spdlog::level::warn;
    if (lower == "error" || lower == "err") return spdlog::level::err;
    if (lower == "critical") return spdlog::level::critical;
    if (lower == "off") return spdlog::level::off;

    return spdlog::level::info;
}

void Logger::init(const std::string& logFile, size_t maxSize, size_t maxFiles, const std::string& logLevel) {
    static std::mutex initMutex;
    std::lock_guard<std::mutex> lock(initMutex);

    if (sLogger != nullptr) {
        return;
    }

    try {
        std::vector<spdlog::sink_ptr> sinks;

        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFile, maxSize, maxFiles));

        sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_mt>());

        sLogger = std::make_shared<spdlog::logger>("global_logger", sinks.begin(), sinks.end());

        sLogger->set_level(parseLogLevel(logLevel));

        sLogger->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

        spdlog::register_logger(sLogger);

        sLogger->flush_on(parseLogLevel(logLevel));
    }
    catch (const spdlog::spdlog_ex& ex) {
        sLogger = spdlog::stdout_logger_mt("fallback_logger");
        sLogger->error("Logger initialization failed: {}", ex.what());
    }
}

void Logger::shutdown() {
	spdlog::shutdown();
}

std::shared_ptr<spdlog::logger> Logger::getLogger() {
    if (sLogger == nullptr) {
        return nullptr;
    }
    return sLogger;
}
