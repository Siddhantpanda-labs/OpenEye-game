#pragma once

#include "eyelib/core/Log.h"

#include <cstdlib>

namespace eyelib {
namespace debug {

inline void FailFast(const char* expr, const char* file, int line, const char* msg = nullptr) {
    if (msg) {
        core::Log::Error(std::string("ASSERT failed: ") + expr + " (" + msg + ") at " + file + ":" + std::to_string(line));
    } else {
        core::Log::Error(std::string("ASSERT failed: ") + expr + " at " + file + ":" + std::to_string(line));
    }
#if defined(_MSC_VER)
    __debugbreak();
#else
    std::abort();
#endif
}

} // namespace debug
} // namespace eyelib

#define EYELIB_ASSERT(expr) \
    do { if (!(expr)) { ::eyelib::debug::FailFast(#expr, __FILE__, __LINE__, nullptr); } } while (0)

#define EYELIB_ASSERT_MSG(expr, msg) \
    do { if (!(expr)) { ::eyelib::debug::FailFast(#expr, __FILE__, __LINE__, msg); } } while (0)

