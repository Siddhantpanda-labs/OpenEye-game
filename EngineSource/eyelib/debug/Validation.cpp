#include "eyelib/debug/Validation.h"

namespace eyelib {
namespace debug {

namespace {
std::mutex g_mutex;
std::vector<ValidationIssue> g_issues;
bool g_breakOnError = true;

static const char* SeverityLabel(ValidationSeverity s) {
    switch (s) {
        case ValidationSeverity::Info: return "INFO";
        case ValidationSeverity::Warning: return "WARN";
        default: return "ERROR";
    }
}
} // namespace

void Validation::Report(ValidationSeverity severity, const std::string& message, const char* file, int line) {
    ValidationIssue issue;
    issue.severity = severity;
    issue.message = message;
    if (file) issue.file = file;
    issue.line = line;
    {
        std::scoped_lock lock(g_mutex);
        g_issues.push_back(issue);
    }
    core::Log::Error(std::string("VALIDATION ") + SeverityLabel(severity) + ": " + message +
                     (file ? (" at " + std::string(file) + ":" + std::to_string(line)) : ""));
}

std::vector<ValidationIssue> Validation::GetIssues() {
    std::scoped_lock lock(g_mutex);
    return g_issues;
}

void Validation::Clear() {
    std::scoped_lock lock(g_mutex);
    g_issues.clear();
}

void Validation::SetBreakOnError(bool enabled) {
    g_breakOnError = enabled;
}

bool Validation::GetBreakOnError() {
    return g_breakOnError;
}

} // namespace debug
} // namespace eyelib
