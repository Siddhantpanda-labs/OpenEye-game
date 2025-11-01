#include "Log.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <cstdarg>
#include <cstdio>

namespace OpenEye {
namespace Core {

LogLevel Log::s_level = LogLevel::Info;
std::unique_ptr<std::ofstream> Log::s_logFile = nullptr;
bool Log::s_initialized = false;

void Log::Initialize(const std::string& logFile) {
    if (s_initialized) {
        return;
    }
    
    s_logFile = std::make_unique<std::ofstream>(logFile, std::ios::app);
    if (!s_logFile->is_open()) {
        std::cerr << "Failed to open log file: " << logFile << std::endl;
        s_logFile.reset();
    }
    
    s_initialized = true;
    Info("Log system initialized");
}

void Log::Shutdown() {
    if (s_initialized && s_logFile) {
        Info("Log system shutting down");
        s_logFile->close();
        s_logFile.reset();
    }
    s_initialized = false;
}

void Log::SetLevel(LogLevel level) {
    s_level = level;
}

LogLevel Log::GetLevel() {
    return s_level;
}

void Log::Trace(const std::string& message) {
    if (s_level <= LogLevel::Trace) {
        LogMessage(LogLevel::Trace, message);
    }
}

void Log::Debug(const std::string& message) {
    if (s_level <= LogLevel::Debug) {
        LogMessage(LogLevel::Debug, message);
    }
}

void Log::Info(const std::string& message) {
    if (s_level <= LogLevel::Info) {
        LogMessage(LogLevel::Info, message);
    }
}

void Log::Warn(const std::string& message) {
    if (s_level <= LogLevel::Warn) {
        LogMessage(LogLevel::Warn, message);
    }
}

void Log::Error(const std::string& message) {
    if (s_level <= LogLevel::Error) {
        LogMessage(LogLevel::Error, message);
    }
}

void Log::Fatal(const std::string& message) {
    LogMessage(LogLevel::Fatal, message);
}

void Log::LogMessage(LogLevel level, const std::string& message) {
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    ss << " [" << GetLevelString(level) << "] " << message;
    
    std::string logLine = ss.str();
    
    // Output to console
    if (level >= LogLevel::Error) {
        std::cerr << logLine << std::endl;
    } else {
        std::cout << logLine << std::endl;
    }
    
    // Output to file
    if (s_logFile && s_logFile->is_open()) {
        *s_logFile << logLine << std::endl;
        s_logFile->flush();
    }
}

std::string Log::FormatString(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    // Determine required buffer size
    va_list argsCopy;
    va_copy(argsCopy, args);
    int size = std::vsnprintf(nullptr, 0, format, argsCopy);
    va_end(argsCopy);
    
    if (size < 0) {
        va_end(args);
        return "";
    }
    
    // Format the string
    std::string result(size + 1, '\0');
    std::vsnprintf(&result[0], size + 1, format, args);
    va_end(args);
    
    result.resize(size);
    return result;
}

std::string Log::GetLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default: return "UNKNOWN";
    }
}

} // namespace Core
} // namespace OpenEye

