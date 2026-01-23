#include "Log.h"
#include "eyelib/core/Log.h"
#include <cstdarg>
#include <cstdio>

namespace eyelib {
namespace api {

void Log::Initialize(const std::string& logFile) {
    eyelib::core::Log::Initialize(logFile);
}

void Log::Shutdown() {
    eyelib::core::Log::Shutdown();
}

void Log::SetLevel(LogLevel level) {
    eyelib::core::Log::SetLevel(static_cast<eyelib::core::LogLevel>(level));
}

LogLevel Log::GetLevel() {
    return static_cast<LogLevel>(eyelib::core::Log::GetLevel());
}

void Log::Trace(const std::string& message) {
    eyelib::core::Log::Trace(message);
}

void Log::Debug(const std::string& message) {
    eyelib::core::Log::Debug(message);
}

void Log::Info(const std::string& message) {
    eyelib::core::Log::Info(message);
}

void Log::Warn(const std::string& message) {
    eyelib::core::Log::Warn(message);
}

void Log::Error(const std::string& message) {
    eyelib::core::Log::Error(message);
}

void Log::Fatal(const std::string& message) {
    eyelib::core::Log::Fatal(message);
}

void Log::TraceF(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list argsCopy;
    va_copy(argsCopy, args);
    int size = std::vsnprintf(nullptr, 0, format, argsCopy);
    va_end(argsCopy);
    
    if (size >= 0) {
        std::string message(size + 1, '\0');
        std::vsnprintf(&message[0], size + 1, format, args);
        message.resize(size);
        eyelib::core::Log::Trace(message);
    }
    va_end(args);
}

void Log::DebugF(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list argsCopy;
    va_copy(argsCopy, args);
    int size = std::vsnprintf(nullptr, 0, format, argsCopy);
    va_end(argsCopy);
    
    if (size >= 0) {
        std::string message(size + 1, '\0');
        std::vsnprintf(&message[0], size + 1, format, args);
        message.resize(size);
        eyelib::core::Log::Debug(message);
    }
    va_end(args);
}

void Log::InfoF(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list argsCopy;
    va_copy(argsCopy, args);
    int size = std::vsnprintf(nullptr, 0, format, argsCopy);
    va_end(argsCopy);
    
    if (size >= 0) {
        std::string message(size + 1, '\0');
        std::vsnprintf(&message[0], size + 1, format, args);
        message.resize(size);
        eyelib::core::Log::Info(message);
    }
    va_end(args);
}

void Log::WarnF(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list argsCopy;
    va_copy(argsCopy, args);
    int size = std::vsnprintf(nullptr, 0, format, argsCopy);
    va_end(argsCopy);
    
    if (size >= 0) {
        std::string message(size + 1, '\0');
        std::vsnprintf(&message[0], size + 1, format, args);
        message.resize(size);
        eyelib::core::Log::Warn(message);
    }
    va_end(args);
}

void Log::ErrorF(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list argsCopy;
    va_copy(argsCopy, args);
    int size = std::vsnprintf(nullptr, 0, format, argsCopy);
    va_end(argsCopy);
    
    if (size >= 0) {
        std::string message(size + 1, '\0');
        std::vsnprintf(&message[0], size + 1, format, args);
        message.resize(size);
        eyelib::core::Log::Error(message);
    }
    va_end(args);
}

void Log::FatalF(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list argsCopy;
    va_copy(argsCopy, args);
    int size = std::vsnprintf(nullptr, 0, format, argsCopy);
    va_end(argsCopy);
    
    if (size >= 0) {
        std::string message(size + 1, '\0');
        std::vsnprintf(&message[0], size + 1, format, args);
        message.resize(size);
        eyelib::core::Log::Fatal(message);
    }
    va_end(args);
}

} // namespace api
} // namespace eyelib
