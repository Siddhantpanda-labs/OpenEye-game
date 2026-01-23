#pragma once

#include <string>

// Forward declaration - core implementation details not exposed
namespace eyelib {
namespace core {
    enum class LogLevel;
    class Log;
}

namespace api {

/**
 * @brief Logging level enumeration
 */
enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Fatal = 5
};

/**
 * @brief Logging API
 * 
 * Provides structured logging with file and console output.
 */
class Log {
public:
    /**
     * @brief Initialize the logging system
     * @param logFile Path to log file (default: "eyelib.log")
     */
    static void Initialize(const std::string& logFile = "eyelib.log");
    
    /**
     * @brief Shutdown the logging system
     */
    static void Shutdown();
    
    /**
     * @brief Set the minimum log level
     * @param level Minimum level to log
     */
    static void SetLevel(LogLevel level);
    
    /**
     * @brief Get the current log level
     * @return Current log level
     */
    static LogLevel GetLevel();
    
    // Logging functions
    static void Trace(const std::string& message);
    static void Debug(const std::string& message);
    static void Info(const std::string& message);
    static void Warn(const std::string& message);
    static void Error(const std::string& message);
    static void Fatal(const std::string& message);
    
    // Format string support - implemented in .cpp to avoid exposing core
    static void TraceF(const char* format, ...);
    static void DebugF(const char* format, ...);
    static void InfoF(const char* format, ...);
    static void WarnF(const char* format, ...);
    static void ErrorF(const char* format, ...);
    static void FatalF(const char* format, ...);
};

} // namespace api
} // namespace eyelib
