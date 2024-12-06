#include "WorkerLogger.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

WorkerLogger::WorkerLogger(const std::string& logger_name, const std::string& log_file, spdlog::level::level_enum level) {
    std::vector<spdlog::sink_ptr> sinks;

    // Sink to write on file
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");  // Pattern senza colori
    sinks.push_back(file_sink);

    // Sink to log on console
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %^[%l]%$ %v");  // Pattern con colori
    sinks.push_back(console_sink);

    // Create a logger with both sinks
    logger = std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end());
    logger->set_level(level);
    // logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
}

void WorkerLogger::debug(const std::string& msg, const std::string& extra) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
    logger->debug("{} - \"{}\"", extra, msg);
#endif
}

void WorkerLogger::info(const std::string& msg, const std::string& extra) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_INFO
    logger->info("{} \"{}\"", extra, msg);
#endif
}

void WorkerLogger::warning(const std::string& msg, const std::string& extra) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_WARN
    logger->warn("{} \"{}\"", extra, msg);
#endif
}

void WorkerLogger::error(const std::string& msg, const std::string& extra) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_ERROR
    logger->error("{} \"{}\"", extra, msg);
#endif
}

void WorkerLogger::critical(const std::string& msg, const std::string& extra) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_CRITICAL
    logger->critical("{} - \"{}\"", extra, msg);
#endif
}

void WorkerLogger::system(const std::string& msg, const std::string& extra) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
    logger->trace("SYSTEM - {} - \"{}\"", extra, msg);
#endif
}
