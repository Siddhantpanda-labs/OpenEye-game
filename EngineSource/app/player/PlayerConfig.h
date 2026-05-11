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

/**
 * When true, `WorldRenderBridge::BuildScene` also constructs and fills a parallel
 * `eyelib::api::rendering::RenderScene` so the dual-stack snapshot stays compile-checked.
 * Default off — enable locally when validating M4.2+ dual fills.
 */
constexpr bool kValidateParallelRenderSceneBuild = false;

/**
 * M4.4 — when true, `WorldRenderBridge` initializes the new `api/rendering` pipeline
 * against the standalone player window and routes Render / RenderSceneOnly / Finish /
 * Blit / fence / Resize / output APIs to it instead of the legacy stack. Implications
 * when enabled (single-device flip):
 *   - Legacy `m_renderer / m_pipeline / m_builtinManager / m_gpuResourceCache` are NOT
 *     constructed; legacy `assetRegistry->SetGPUResolver` / `SetRenderDevice` are skipped.
 *   - `GetDevice()` returns null (the new device is only safe to consume through
 *     `eyelib::api::rendering::RenderDevice`, not the legacy `eyelib::api::RenderDevice`).
 *   - Streaming + XR call sites become one-shot warn-and-skip (M4.5 / M4.6).
 *   - Editor "Embedded" mode (`Initialize(void* sharedDevice, ...)`) ignores this flag
 *     and stays on legacy until M3.9 lands the editor-side device flip.
 *
 * Default off; enable to dogfood the new path on standalone player. Overlay (ImGui) is
 * not yet wired to the new device — gaining UI / streaming back lands at M4.7+.
 */
constexpr bool kUseNewRenderPipeline = false;

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

/** Asset roots, timestep defaults, headless/overlay consistency. */
void ApplyRuntimeDefaults(RuntimeOptions& options);
/** Profile dev | shipped; returns false and sets \a error if profile name is invalid. */
bool ApplyRuntimeProfile(RuntimeOptions& options, std::string& error);

} // namespace openeye::player
