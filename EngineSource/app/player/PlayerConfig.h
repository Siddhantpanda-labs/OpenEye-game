#pragma once

#include "api/render/RenderBackend.h"

#include <string>
#include <vector>

namespace openeye::player {

/**
 * When false: render runs on the main thread (BuildScene then RenderSceneOnly then present inline).
 * When true:  render runs on a dedicated thread; main thread submits via RenderFrameSync.
 * Set to true once threading/flicker is resolved to regain CPU overlap and smoother frame pacing.
 */
constexpr bool kUseRenderThread = false;

enum class TimeMode {
    Realtime,
    Fixed,
    Replay
};

struct RuntimeOptions {
    std::string configPath;
    std::string profile;
    std::string scenePath = "scenes/main.json";
    std::vector<std::string> assetRoots;
    std::string logPath = "OpenEyePlayer.log";
    std::string title = "OpenEye Player";
    int width = 1280;
    int height = 720;
    float fixedTimestep = 0.0f;
    float timeScale = 1.0f;
    float replayDelta = 0.0f;
    int maxSubsteps = 8;
    bool headless = false;
    bool vsync = true;
    bool paused = false;
    bool stepOnce = false;
    bool stats = false;
    bool strictAssets = false;
    bool overlay = false;
    bool headlessSet = false;
    bool statsSet = false;
    bool strictAssetsSet = false;
    bool overlaySet = false;
    bool timeModeSet = false;
    TimeMode timeMode = TimeMode::Realtime;
    bool showHelp = false;
    /** Render backend (e.g. from --renderer opengl|diligent). */
    eyelib::render::RenderBackend renderBackend = eyelib::render::RenderBackend::Default;
    /** If > 0, exit after this many frames (for smoke tests). */
    int maxFrames = 0;
};

void PrintUsage(const char* exeName);
std::string FindConfigPath(int argc, char** argv);
bool LoadConfig(const std::string& path, RuntimeOptions& options, std::string& error);
bool ParseArgs(int argc, char** argv, RuntimeOptions& options, std::string& error);

} // namespace openeye::player
