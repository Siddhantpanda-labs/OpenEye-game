#include "core/GLDrawDebugLabel.h"
#include <string>
#include <mutex>

namespace eyelib {
namespace core {

namespace {
std::string s_glDrawDebugLabel;
std::mutex s_glDrawDebugLabelMutex;
}

void SetGLDrawDebugLabel(const char* label) {
    std::lock_guard<std::mutex> lock(s_glDrawDebugLabelMutex);
    s_glDrawDebugLabel = label ? label : "";
}

const char* GetGLDrawDebugLabel() {
    std::lock_guard<std::mutex> lock(s_glDrawDebugLabelMutex);
    return s_glDrawDebugLabel.empty() ? nullptr : s_glDrawDebugLabel.c_str();
}

} // namespace core
} // namespace eyelib
