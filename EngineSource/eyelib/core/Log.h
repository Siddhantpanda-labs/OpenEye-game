#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <memory>

namespace eyelib {
namespace core {

/**
 * @brief Logging system with multiple severity levels
 * 
 * Provides structured logging with file and console output.
 * Thread-safe for basic operations.
 */
enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Fatal = 5
};

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
    
    // Format string support (printf-style)
    template<typename... Args>
    static void TraceF(const char* format, Args... args) {
        if (GetLevel() <= LogLevel::Trace) {
            LogMessage(LogLevel::Trace, FormatString(format, args...));
        }
    }
    
    template<typename... Args>
    static void DebugF(const char* format, Args... args) {
        if (GetLevel() <= LogLevel::Debug) {
            LogMessage(LogLevel::Debug, FormatString(format, args...));
        }
    }
    
    template<typename... Args>
    static void InfoF(const char* format, Args... args) {
        if (GetLevel() <= LogLevel::Info) {
            LogMessage(LogLevel::Info, FormatString(format, args...));
        }
    }
    
    template<typename... Args>
    static void WarnF(const char* format, Args... args) {
        if (GetLevel() <= LogLevel::Warn) {
            LogMessage(LogLevel::Warn, FormatString(format, args...));
        }
    }
    
    template<typename... Args>
    static void ErrorF(const char* format, Args... args) {
        if (GetLevel() <= LogLevel::Error) {
            LogMessage(LogLevel::Error, FormatString(format, args...));
        }
    }
    
    template<typename... Args>
    static void FatalF(const char* format, Args... args) {
        LogMessage(LogLevel::Fatal, FormatString(format, args...));
    }

private:
    static void LogMessage(LogLevel level, const std::string& message);
    static std::string FormatString(const char* format, ...);
    static std::string GetLevelString(LogLevel level);
    
    static LogLevel s_level;
    static std::unique_ptr<std::ofstream> s_logFile;
    static bool s_initialized;
};

} // namespace core
} // namespace eyelib
