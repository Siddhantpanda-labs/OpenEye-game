#pragma once

#include "eyelib/debug/Validation.h"

#include <string>
#include <vector>

namespace eyelib {
namespace api {
namespace debug {

using ValidationSeverity = eyelib::debug::ValidationSeverity;
using ValidationIssue = eyelib::debug::ValidationIssue;

class Validation {
public:
    static void Report(ValidationSeverity severity, const std::string& message, const char* file = nullptr, int line = 0) {
        eyelib::debug::Validation::Report(severity, message, file, line);
    }
    static std::vector<ValidationIssue> GetIssues() {
        return eyelib::debug::Validation::GetIssues();
    }
    static void Clear() {
        eyelib::debug::Validation::Clear();
    }
    static void SetBreakOnError(bool enabled) {
        eyelib::debug::Validation::SetBreakOnError(enabled);
    }
    static bool GetBreakOnError() {
        return eyelib::debug::Validation::GetBreakOnError();
    }
};

} // namespace debug
} // namespace api
} // namespace eyelib

// Re-export engine assertions for API consumers.
#define EYELIB_API_ASSERT(expr) EYELIB_ASSERT(expr)
#define EYELIB_API_ASSERT_MSG(expr, msg) EYELIB_ASSERT_MSG(expr, msg)
