#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/fmt/fmt.h"
#include <string>
#include "WorkerLogger.h"

// Constructor to initialize the WorkerLogger with a logger name, log file, and log level
WorkerLogger::WorkerLogger(const std::string& logger_name, const std::string& log_file, spdlog::level::level_enum level) {
    // Create a logger
    logger = spdlog::basic_logger_mt(logger_name, log_file);
    logger->set_level(level);

    // Set a custom formatter
    logger->set_pattern("%Y-%m-%d %H:%M:%S.%e - %l - %v");
}

// Logging method for debug level
void WorkerLogger::debug(const std::string& msg, const std::string& extra) {
    SPDLOG_DEBUG("{} - \"{}\"", extra, msg);
}

// Logging method for info level
void WorkerLogger::info(const std::string& msg, const std::string& extra) {
    SPDLOG_INFO("{} - \"{}\"", extra, msg);
}

// Logging method for warning level
void WorkerLogger::warning(const std::string& msg, const std::string& extra) {
    SPDLOG_WARN("{} - \"{}\"", extra, msg);
}

// Logging method for error level
void WorkerLogger::error(const std::string& msg, const std::string& extra) {
    SPDLOG_ERROR("{} - \"{}\"", extra, msg);
}

// Logging method for critical level
void WorkerLogger::critical(const std::string& msg, const std::string& extra) {
    SPDLOG_CRITICAL("{} - \"{}\"", extra, msg);
}

// Logging method for system level
void WorkerLogger::system(const std::string& msg, const std::string& extra) {
    SPDLOG_TRACE("SYSTEM - {} - \"{}\"", extra, msg);
}

// Helper method to log system-level messages
void WorkerLogger::log_system(const std::string& msg, const std::string& extra) {
    SPDLOG_TRACE("SYSTEM - {} - \"{}\"", extra, msg);
}
