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

    // Set the level to trace to ensure all logs are captured
    spdlog::set_level(spdlog::level::trace);
}

// Logging method for debug level
void WorkerLogger::debug(const std::string& msg, const std::string& extra) {
    logger->debug("{} - \"{}\"", extra, msg);
}

// Logging method for info level
void WorkerLogger::info(const std::string& msg, const std::string& extra) {
    logger->info("{} - \"{}\"", extra, msg);
}

// Logging method for warning level
void WorkerLogger::warning(const std::string& msg, const std::string& extra) {
    logger->warn("{} - \"{}\"", extra, msg);
}

// Logging method for error level
void WorkerLogger::error(const std::string& msg, const std::string& extra) {
    logger->error("{} - \"{}\"", extra, msg);
}

// Logging method for critical level
void WorkerLogger::critical(const std::string& msg, const std::string& extra) {
    logger->critical("{} - \"{}\"", extra, msg);
}

// Logging method for system level
void WorkerLogger::system(const std::string& msg, const std::string& extra) {
    logger->trace("SYSTEM - {} - \"{}\"", extra, msg);
}

// Helper method to log system-level messages
void WorkerLogger::log_system(const std::string& msg, const std::string& extra) {
    logger->trace("SYSTEM - {} - \"{}\"", extra, msg);
}