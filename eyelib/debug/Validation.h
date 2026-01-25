#pragma once

#include "eyelib/core/Log.h"

#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace eyelib {
namespace debug {

enum class ValidationSeverity {
    Info,
    Warning,
    Error
};

struct ValidationIssue {
    ValidationSeverity severity = ValidationSeverity::Error;
    std::string message;
    std::string file;
    int line = 0;
};

class Validation {
public:
    static void Report(ValidationSeverity severity, const std::string& message, const char* file = nullptr, int line = 0);
    static std::vector<ValidationIssue> GetIssues();
    static void Clear();
    static void SetBreakOnError(bool enabled);
    static bool GetBreakOnError();
};

inline void FailFast(const char* expr, const char* file, int line, const char* msg = nullptr) {
    const std::string message = msg ? (std::string(expr) + " (" + msg + ")") : std::string(expr);
    Validation::Report(ValidationSeverity::Error, std::string("ASSERT failed: ") + message, file, line);
#if defined(_MSC_VER)
    if (Validation::GetBreakOnError()) {
        __debugbreak();
    }
#else
    if (Validation::GetBreakOnError()) {
        std::abort();
    }
#endif
}

} // namespace debug
} // namespace eyelib

#define EYELIB_ASSERT(expr) \
    do { if (!(expr)) { ::eyelib::debug::FailFast(#expr, __FILE__, __LINE__, nullptr); } } while (0)

#define EYELIB_ASSERT_MSG(expr, msg) \
    do { if (!(expr)) { ::eyelib::debug::FailFast(#expr, __FILE__, __LINE__, msg); } } while (0)

