#include "PlayerApp.h"

#include "AssetBinding.h"
#include "Overlay.h"
#include "PlayerConfig.h"
#include "WorldAudio.h"
#include "WorldRender.h"
#include "WorldPhysics.h"
#include "WorldAnimation.h"
#include "WorldScripting.h"
#include "RenderFrameSync.h"

#include "api/assets/All.h"
#include "api/core/All.h"
#include "api/platform/All.h"
#include "api/world/All.h"
#include "eyelib/assets/ShaderWatcher.h"

#include <functional>
#include <iostream>
#include <utility>
#include <cstdio>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace openeye::player {

namespace {

class ScopedEngine {
public:
    bool Initialize(const eyelib::api::core::Engine::Config& config) {
        m_initialized = eyelib::api::core::Engine::Initialize(config);
        return m_initialized;
    }

    ~ScopedEngine() {
        if (m_initialized) {
            eyelib::api::core::Engine::Shutdown();
        }
    }

private:
    bool m_initialized = false;
};

class ScopedWindow {
public:
    bool Initialize(eyelib::api::Window& window, const eyelib::api::WindowConfig& config) {
        if (!window.Initialize(config)) {
            return false;
        }
        m_window = &window;
        return true;
    }

    ~ScopedWindow() {
        if (m_window) {
            m_window->Shutdown();
        }
    }

private:
    eyelib::api::Window* m_window = nullptr;
};

class ScopedOverlay {
public:
    explicit ScopedOverlay(RuntimeOverlay& overlay) : m_overlay(&overlay) {}

    ~ScopedOverlay() {
        if (m_overlay) {
            m_overlay->Shutdown();
        }
    }

private:
    RuntimeOverlay* m_overlay = nullptr;
};

std::string NormalizePath(const std::string& path) {
    using namespace eyelib::api::platform::filesystem;
    if (path.empty()) {
        return {};
    }
    std::string absolute = GetAbsolutePath(path);
    return eyelib::api::platform::filesystem::NormalizePath(absolute);
}

std::vector<std::string> NormalizeRoots(const std::vector<std::string>& roots) {
    std::vector<std::string> out;
    out.reserve(roots.size());
    for (const auto& root : roots) {
        out.push_back(NormalizePath(root));
    }
    return out;
}

static bool IsAbsolutePath(const std::string& path) {
    if (path.empty()) return false;
#ifdef _WIN32
    if (path.size() > 1 && path[1] == ':') return true;
#endif
    return path[0] == '/' || path[0] == '\\';
}

static void AttachConsoleIfNeeded() {
#ifdef _WIN32
    if (GetConsoleWindow() == NULL && AllocConsole()) {
        (void)freopen("CONOUT$", "w", stdout);
        (void)freopen("CONOUT$", "w", stderr);
        std::ios_base::sync_with_stdio(true);
    }
#endif
}

static void ResolveLogPathToExeDir(RuntimeOptions& options) {
    using namespace eyelib::api::platform::filesystem;
    std::string exePath = GetExecutablePath();
    if (exePath.empty()) return;
    std::string exeDir = GetDirectory(exePath);
    if (exeDir.empty()) return;
    if (!IsAbsolutePath(options.logPath)) {
        if (!exeDir.empty() && exeDir.back() != '/' && exeDir.back() != '\\')
            exeDir += "/";
        std::string full = GetAbsolutePath(exeDir + options.logPath);
        options.logPath = eyelib::api::platform::filesystem::NormalizePath(full);
    }
}

void ApplyDefaults(RuntimeOptions& options) {
    if (options.assetRoots.empty()) {
        options.assetRoots.emplace_back("assets");
    }

    if (options.fixedTimestep > 0.0f && !options.timeModeSet) {
        options.timeMode = TimeMode::Fixed;
        options.timeModeSet = true;
    }

    if (options.timeModeSet
        && options.timeMode == TimeMode::Replay
        && options.replayDelta <= 0.0f) {
        options.replayDelta = 1.0f / 60.0f;
    }

    if (options.headless && options.overlay) {
        options.overlay = false;
    }
}

bool ApplyProfile(RuntimeOptions& options, std::string& error) {
    if (options.profile.empty()) {
        return true;
    }
    if (options.profile == "dev") {
        if (!options.overlaySet) options.overlay = true;
        if (!options.statsSet) options.stats = true;
        if (!options.strictAssetsSet) options.strictAssets = false;
        if (!options.headlessSet) options.headless = false;
        return true;
    }
    if (options.profile == "shipped") {
        if (!options.overlaySet) options.overlay = false;
        if (!options.statsSet) options.stats = false;
        if (!options.strictAssetsSet) options.strictAssets = true;
        if (!options.headlessSet) options.headless = false;
        return true;
    }
    error = "Invalid profile: " + options.profile;
    return false;
}

void LogOptions(const RuntimeOptions& options) {
    eyelib::api::core::Log::Info("OpenEye unified runtime player starting.");
    eyelib::api::core::Log::InfoF("Scene: %s", options.scenePath.c_str());
    if (options.headless) {
        eyelib::api::core::Log::Info("Mode: headless");
    }
    if (options.timeModeSet) {
        const char* mode = "realtime";
        if (options.timeMode == TimeMode::Fixed) {
            mode = "fixed";
        } else if (options.timeMode == TimeMode::Replay) {
            mode = "replay";
        }
        eyelib::api::core::Log::InfoF("Time mode: %s", mode);
    }
    for (const auto& root : options.assetRoots) {
        eyelib::api::core::Log::InfoF("Asset root: %s", root.c_str());
        if (!eyelib::api::platform::filesystem::FileExists(root)) {
            eyelib::api::core::Log::WarnF("Asset root does not exist: %s", root.c_str());
        }
    }
}

} // namespace

int RunPlayer(int argc, char** argv) {
    AttachConsoleIfNeeded();

    RuntimeOptions options;
    std::string error;
    const std::string configPath = FindConfigPath(argc, argv);
    if (!configPath.empty()) {
        if (!LoadConfig(configPath, options, error)) {
            if (!error.empty()) {
                std::cerr << error << "\n";
            }
            return static_cast<int>(ExitCode::ConfigError);
        }
    }
    if (!ParseArgs(argc, argv, options, error)) {
        if (!error.empty()) {
            std::cerr << error << "\n";
        }
        PrintUsage(argv[0]);
        return static_cast<int>(ExitCode::InvalidArgs);
    }
    if (options.showHelp) {
        PrintUsage(argv[0]);
        return static_cast<int>(ExitCode::Ok);
    }

    if (!ApplyProfile(options, error)) {
        if (!error.empty()) {
            std::cerr << error << "\n";
        }
        return static_cast<int>(ExitCode::InvalidArgs);
    }

    ApplyDefaults(options);
    options.scenePath = NormalizePath(options.scenePath);
    options.assetRoots = NormalizeRoots(options.assetRoots);
    ResolveLogPathToExeDir(options);

    eyelib::api::core::Engine::Config config;
    config.enableLogging = true;
    config.logFile = options.logPath;
    config.headless = options.headless;

    ScopedEngine engine;
    if (!engine.Initialize(config)) {
        std::cerr << "Failed to initialize engine.\n";
        return static_cast<int>(ExitCode::EngineInitFailed);
    }

    LogOptions(options);

    if (!eyelib::api::platform::filesystem::FileExists(options.scenePath)) {
        eyelib::api::core::Log::ErrorF("Scene file not found: %s", options.scenePath.c_str());
        return static_cast<int>(ExitCode::SceneMissing);
    }

    eyelib::api::world::World world;
    if (options.fixedTimestep > 0.0f) {
        world.SetFixedTimestep(options.fixedTimestep);
        if (!options.timeModeSet) {
            options.timeMode = TimeMode::Fixed;
            options.timeModeSet = true;
        }
    }
    world.SetMaxSubsteps(options.maxSubsteps);
    world.SetTimeScale(options.timeScale);
    if (options.timeModeSet) {
        ::eyelib::world::WorldScheduler::TimeMode mode = ::eyelib::world::WorldScheduler::TimeMode::Realtime;
        if (options.timeMode == TimeMode::Fixed) {
            mode = ::eyelib::world::WorldScheduler::TimeMode::Fixed;
        } else if (options.timeMode == TimeMode::Replay) {
            mode = ::eyelib::world::WorldScheduler::TimeMode::Replay;
        }
        world.SetTimeMode(mode);
    }
    if (options.replayDelta > 0.0f) {
        world.SetReplayDelta(options.replayDelta);
    }
    if (options.paused) {
        world.SetPaused(true);
    }
    if (options.stepOnce) {
        world.StepOnce();
    }

    if (!world.LoadScene(options.scenePath)) {
        eyelib::api::core::Log::ErrorF("Failed to load scene: %s", options.scenePath.c_str());
        return static_cast<int>(ExitCode::SceneLoadFailed);
    }

    eyelib::api::Window window;
    ScopedWindow windowGuard;
    if (!options.headless) {
        eyelib::api::WindowConfig windowConfig;
        windowConfig.title = options.title;
        windowConfig.width = options.width;
        windowConfig.height = options.height;
        windowConfig.vsync = options.vsync;
        if (!windowGuard.Initialize(window, windowConfig)) {
            eyelib::api::core::Log::Error("Failed to initialize window.");
            return static_cast<int>(ExitCode::WindowInitFailed);
        }
    }

    RuntimeOverlay overlay;
    if (!options.headless && options.overlay) {
        overlay.Initialize(window);
    }
    ScopedOverlay overlayGuard(overlay);

    WorldRenderBridge renderBridge;
    if (!options.headless) {
        if (!renderBridge.Initialize(window,
                                     static_cast<uint32_t>(window.Width()),
                                     static_cast<uint32_t>(window.Height()))) {
            eyelib::api::core::Log::Error("Failed to initialize renderer or pipeline.");
            return static_cast<int>(ExitCode::RenderInitFailed);
        }
    }

    eyelib::api::AssetRegistry assetRegistry;
    const bool allowGpuAssets = !options.headless;
    const auto assetStats = BindAssets(world, options.assetRoots, assetRegistry, allowGpuAssets);
    if (assetStats.referenced > 0) {
        eyelib::api::core::Log::InfoF(
            "Assets: referenced=%d loaded=%d missing=%d unknownType=%d",
            assetStats.referenced, assetStats.loaded, assetStats.missing, assetStats.unknownType
        );
    }
    if (options.strictAssets && (assetStats.missing > 0 || assetStats.unknownType > 0)) {
        eyelib::api::core::Log::Error("Strict asset mode: missing or unknown assets detected.");
        return static_cast<int>(ExitCode::AssetStrictFailed);
    }

    WorldPhysicsBridge physicsBridge;
    if (!physicsBridge.Initialize()) {
        eyelib::api::core::Log::Error("Failed to initialize physics world.");
        return static_cast<int>(ExitCode::PhysicsInitFailed);
    }
    if (!physicsBridge.BuildFromWorld(world)) {
        eyelib::api::core::Log::Warn("Physics binding reported issues; continuing.");
    }

    WorldAnimationBridge animationBridge;
    animationBridge.BuildFromWorld(world, assetRegistry);

    WorldScriptingBridge scriptingBridge;
    scriptingBridge.SetPhysicsBridge(&physicsBridge);
    scriptingBridge.SetAnimationBridge(&animationBridge);
    scriptingBridge.BuildFromWorld(world, options.assetRoots);

    WorldAudioBridge audioBridge;
    if (!options.headless) {
        if (!audioBridge.Initialize()) {
            eyelib::api::core::Log::Warn("Audio bridge init failed; continuing without audio.");
        } else {
            audioBridge.BuildFromWorld(world);
        }
    }

    std::unique_ptr<eyelib::assets::ShaderWatcher> shaderWatcher;
    if (!options.headless) {
        shaderWatcher = std::make_unique<eyelib::assets::ShaderWatcher>();
        std::string exePath = eyelib::api::platform::filesystem::GetExecutablePath();
        std::string exeDir = eyelib::api::platform::filesystem::GetDirectory(exePath);
        std::string runtimeAssets = exeDir.empty()
            ? eyelib::api::platform::filesystem::GetAbsolutePath("assets")
            : eyelib::api::platform::filesystem::NormalizePath(
                exeDir + (exeDir.back() == '/' || exeDir.back() == '\\' ? "" : "/") + "assets");
        std::string sourceAssets;
        const std::vector<std::string> candidates = {
            eyelib::api::platform::filesystem::GetAbsolutePath("../../assets"),
            eyelib::api::platform::filesystem::GetAbsolutePath("../../../assets"),
            eyelib::api::platform::filesystem::GetAbsolutePath("../../../../assets"),
            eyelib::api::platform::filesystem::GetAbsolutePath("../assets"),
            eyelib::api::platform::filesystem::GetAbsolutePath("assets"),
            runtimeAssets
        };
        for (const auto& candidate : candidates) {
            std::string shadersDir = candidate;
            if (!shadersDir.empty() && shadersDir.back() != '/' && shadersDir.back() != '\\')
                shadersDir += "/shaders";
            else
                shadersDir += "shaders";
            if (eyelib::api::platform::filesystem::FileExists(shadersDir)) {
                sourceAssets = candidate;
                break;
            }
        }
        if (sourceAssets.empty()) sourceAssets = runtimeAssets;
        shaderWatcher->SetSourceAssetsDir(sourceAssets);
        shaderWatcher->SetRuntimeAssetsDir(runtimeAssets);
        eyelib::api::core::Log::InfoF("ShaderWatcher source: %s", sourceAssets.c_str());
        eyelib::api::core::Log::InfoF("ShaderWatcher runtime: %s", runtimeAssets.c_str());
    }

    struct RuntimeStats {
        float accum = 0.0f;
        int frames = 0;
    } stats;

    // Job pool for parallelizable work (physics step, animation update). 0 = use hardware concurrency.
    eyelib::api::core::ThreadPool jobPool(0);

    RenderFrameSync frameSync;
    std::thread renderThread;

    if (!options.headless) {
        // Warmup: run one BuildScene + RenderSceneOnly on the main thread (which still has the GL
        // context) so all materials/shaders used by the scene are loaded and compiled here. After
        // we start the render thread, the context moves there and the game thread must not touch GL.
        renderBridge.BuildScene(world, options, &animationBridge, &assetRegistry,
            frameSync.GetSceneBuffer(0), frameSync.GetFrameAllocator(0));
        renderBridge.RenderSceneOnly(frameSync.GetSceneBuffer(0),
            static_cast<uint32_t>(window.Width()),
            static_cast<uint32_t>(window.Height()));

        renderThread = std::thread([&window, &frameSync, &renderBridge, &overlay, &options]() {
            window.MakeContextCurrent();
            for (;;) {
                std::optional<RenderFrameSync::AcquiredFrame> frame = frameSync.AcquireFrame();
                if (!frame) {
                    break;
                }
                // Robust: use current framebuffer size from the system (single source of truth).
                window.RefreshFramebufferSize();
                const int fbWidth = window.Width();
                const int fbHeight = window.Height();
                if (fbWidth <= 0 || fbHeight <= 0) {
                    frameSync.ReleaseFrame(frame->slot);
                    continue;
                }
                window.ApplyVsync();
                const uint32_t w = static_cast<uint32_t>(fbWidth);
                const uint32_t h = static_cast<uint32_t>(fbHeight);
                renderBridge.ApplyResize(w, h);
                const bool rendered = renderBridge.RenderSceneOnly(frameSync.GetSceneBuffer(frame->slot), w, h);
                if (rendered) {
                    if (options.overlay && overlay.IsReady()) {
                        overlay.Render(frame->overlay);
                    }
                    window.SwapBuffers();
                }
                frameSync.ReleaseFrame(frame->slot);
            }
        });
    }

    auto update = [&world, &window, &options, &stats, &physicsBridge, &animationBridge,
                   &scriptingBridge, &audioBridge, &shaderWatcher, &jobPool,
                   &frameSync, &renderBridge, &overlay, &assetRegistry](float deltaSeconds) {
        if (!options.headless) {
            if (shaderWatcher) shaderWatcher->Tick();
            window.PollEvents();
            if (window.ShouldClose()) {
                eyelib::api::core::Engine::RequestShutdown();
                return;
            }
        }

        // Read phase: no world mutation yet (observation is built on demand in sync).
        // Jobs phase: run physics and animation in parallel (read from cached/observation, write to job-local state).
        const float fixedStep = options.fixedTimestep > 0.0f ? options.fixedTimestep : 0.0f;
        jobPool.Submit([&physicsBridge, deltaSeconds, maxSub = options.maxSubsteps, fixedStep]() {
            physicsBridge.StepPhysicsOnly(deltaSeconds, maxSub, fixedStep);
        });
        jobPool.Submit([&animationBridge, deltaSeconds]() {
            animationBridge.Update(deltaSeconds);
        });
        jobPool.Wait();

        // Write phase: sync job results to world, then main-thread-only systems.
        physicsBridge.SyncTransformsToWorld(world);
        if (!options.headless) {
            audioBridge.Update(world, deltaSeconds);
        }
        scriptingBridge.Update(world, options.assetRoots, deltaSeconds);
        world.Update(deltaSeconds);

        if (options.stats) {
            stats.accum += deltaSeconds;
            stats.frames += 1;
            if (stats.accum >= 1.0f) {
                const float fps = eyelib::api::core::Time::FPS();
                const size_t entities = world.GetEntityCount();
                eyelib::api::core::Log::InfoF("Stats: fps=%.1f entities=%zu", fps, entities);
                stats.accum = 0.0f;
                stats.frames = 0;
            }
        }

        // Submit frame to render thread (game thread never touches GL after this).
        if (!options.headless) {
            OverlaySnapshot snap;
            snap.fps = eyelib::api::core::Time::FPS();
            snap.frameTimeMs = eyelib::api::core::Time::DeltaTime() * 1000.0f;
            snap.entityCount = world.GetEntityCount();
            snap.scenePath = options.scenePath;
            const int slot = frameSync.GetFillSlot();
            if (slot < 0) {
                return;
            }
            renderBridge.BuildScene(world, options, &animationBridge, &assetRegistry,
                frameSync.GetSceneBuffer(slot), frameSync.GetFrameAllocator(slot));
            const int w = window.Width();
            const int h = window.Height();
            frameSync.SubmitFrame(slot,
                static_cast<uint32_t>(w > 0 ? w : 1),
                static_cast<uint32_t>(h > 0 ? h : 1),
                snap);
        }
    };

    eyelib::api::core::Engine::Run(update, nullptr);

    if (!options.headless && renderThread.joinable()) {
        frameSync.SignalShutdown();
        renderThread.join();
    }

    if (!options.headless) {
        audioBridge.Shutdown();
        // GL context was on the render thread; make it current here so pipeline/renderer shutdown can destroy GL resources
        window.MakeContextCurrent();
    }
    renderBridge.Shutdown();
    try {
        physicsBridge.Shutdown();
    } catch (const std::exception& e) {
        eyelib::api::core::Log::ErrorF("Physics shutdown exception: %s", e.what());
    } catch (...) {
        eyelib::api::core::Log::Error("Physics shutdown unknown exception");
    }

    eyelib::api::core::Log::Info("Shutdown sequence completed");
    // Explicit engine shutdown so "Shutting down Eyelib Engine..." and "Log system shutting down" appear in order.
    // ScopedEngine destructor will no-op because Shutdown() sets s_initialized = false.
    eyelib::api::core::Engine::Shutdown();

    return static_cast<int>(ExitCode::Ok);
}

} // namespace openeye::player
