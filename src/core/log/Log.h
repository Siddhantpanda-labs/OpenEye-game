#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <memory>

namespace OpenEye {
namespace Core {

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
    static void Initialize(const std::string& logFile = "openeye.log");
    static void Shutdown();
    
    static void SetLevel(LogLevel level);
    static LogLevel GetLevel();
    
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

// Convenience macros
#define OE_TRACE(msg) OpenEye::Core::Log::Trace(msg)
#define OE_DEBUG(msg) OpenEye::Core::Log::Debug(msg)
#define OE_INFO(msg) OpenEye::Core::Log::Info(msg)
#define OE_WARN(msg) OpenEye::Core::Log::Warn(msg)
#define OE_ERROR(msg) OpenEye::Core::Log::Error(msg)
#define OE_FATAL(msg) OpenEye::Core::Log::Fatal(msg)

#define OE_TRACE_F(fmt, ...) OpenEye::Core::Log::TraceF(fmt, __VA_ARGS__)
#define OE_DEBUG_F(fmt, ...) OpenEye::Core::Log::DebugF(fmt, __VA_ARGS__)
#define OE_INFO_F(fmt, ...) OpenEye::Core::Log::InfoF(fmt, __VA_ARGS__)
#define OE_WARN_F(fmt, ...) OpenEye::Core::Log::WarnF(fmt, __VA_ARGS__)
#define OE_ERROR_F(fmt, ...) OpenEye::Core::Log::ErrorF(fmt, __VA_ARGS__)
#define OE_FATAL_F(fmt, ...) OpenEye::Core::Log::FatalF(fmt, __VA_ARGS__)

} // namespace Core
} // namespace OpenEye

