#ifndef WORKERLOGGER_H
#define WORKERLOGGER_H

#include <iostream>
#include <string>
#include <memory>

class WorkerLogger {
public:
    // Define a new custom log level
    static const int SYSTEM_LEVEL_NUM = 10;

    // Constructor to initialize the WorkerLogger with a logger name and log level
    WorkerLogger(const std::string& logger_name = "my_logger");

    // Logging methods for different severity levels
    void debug(const std::string& msg, const std::string& extra = "");
    void info(const std::string& msg, const std::string& extra = "");
    void warning(const std::string& msg, const std::string& extra = "");
    void error(const std::string& msg, const std::string& extra = "");
    void critical(const std::string& msg, const std::string& extra = "");
    void system(const std::string& msg, const std::string& extra = "");

private:
    std::string logger_name;  // Logger name

    // Helper method to log system-level messages
    void log_system(const std::string& msg, const std::string& extra);
};

#endif // WORKERLOGGER_H

