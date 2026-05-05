/**
 * @file tienet.cpp
 * @brief Main Tienet library implementation
 */

#include "tienet.hpp"
#include "tienet/media/encoder.hpp"
#include "tienet/media/decoder.hpp"
#include <iostream>
#include <chrono>

namespace tienet {

// Global state
static bool g_initialized = false;
static std::chrono::steady_clock::time_point g_startTime;

bool initialize() {
    if (g_initialized) {
        return true;
    }
    
    g_startTime = std::chrono::steady_clock::now();
    g_initialized = true;
    
    std::cout << "🚀 Tienet initialized successfully" << std::endl;
    return true;
}

void shutdown() {
    if (!g_initialized) {
        return;
    }
    
    g_initialized = false;
    std::cout << "🛑 Tienet shutdown complete" << std::endl;
}

const char* getVersion() {
    return "1.0.0";
}

const char* getBuildInfo() {
    static std::string buildInfo;
    if (buildInfo.empty()) {
        buildInfo = "Tienet v" + std::string(getVersion()) + " - Built on " + __DATE__ + " " + __TIME__;
#ifdef _WIN32
        buildInfo += " (Windows)";
#elif defined(__linux__)
        buildInfo += " (Linux)";
#elif defined(__APPLE__)
        buildInfo += " (macOS)";
#else
        buildInfo += " (Unknown)";
#endif
    }
    return buildInfo.c_str();
}

} // namespace tienet
