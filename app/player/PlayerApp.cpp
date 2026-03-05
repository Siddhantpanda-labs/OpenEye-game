#include "PlayerApp.h"

#include "GameRuntime.h"
#include "Overlay.h"
#include "PlayerConfig.h"

#include "api/core/All.h"
#include "api/physics/PhysicsModule.h"
#include "api/platform/All.h"
#include "api/world/All.h"
#include "api/assets/ShaderWatcher.h"
#include "api/render/RenderBackend.h"

#include <iostream>
#include <cstdio>

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

    eyelib::api::Engine::RegisterSubsystem(std::make_unique<eyelib::api::PhysicsModule>());
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
        windowConfig.useNativeApiOnly = (options.renderBackend == eyelib::render::RenderBackend::Diligent);
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

    eyelib::api::AssetRegistry assetRegistry;
    openeye::player::GameRuntime gameRuntime;
    const uint32_t initWidth = options.headless ? 0u : static_cast<uint32_t>(window.Width());
    const uint32_t initHeight = options.headless ? 0u : static_cast<uint32_t>(window.Height());
    if (!gameRuntime.Initialize(world, assetRegistry, options.assetRoots,
                                options.headless ? nullptr : &window,
                                initWidth, initHeight,
                                options.renderBackend,
                                eyelib::api::RenderDevice(),
                                &options)) {
        if (options.headless) {
            eyelib::api::core::Log::Error("Failed to initialize game runtime (headless).");
            return static_cast<int>(ExitCode::EngineInitFailed);
        }
        eyelib::api::core::Log::Error("Failed to initialize renderer or game runtime.");
        return static_cast<int>(ExitCode::RenderInitFailed);
    }

    const auto assetStats = gameRuntime.GetAssetBindingStats();
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

    struct RuntimeStats { float accum = 0.0f; int frames = 0; } stats;
    int frameCount = 0;
    eyelib::api::platform::time::FramePacer framePacer;
    if (!options.headless && !options.vsync)
        framePacer.SetTargetFPS(60.0);

    auto update = [&](float deltaSeconds) {
        if (!options.headless && !options.vsync)
            framePacer.WaitForNextFrame();
        world.SetFrameSnapshot(nullptr);
        if (options.maxFrames > 0) {
            frameCount += 1;
            if (frameCount >= options.maxFrames) {
                eyelib::api::core::Log::InfoF("Smoke test: %d frames completed.", options.maxFrames);
                eyelib::api::core::Engine::RequestShutdown();
                return;
            }
        }
        if (!options.headless) {
            if (shaderWatcher) {
                const int copied = shaderWatcher->Tick();
                if (copied > 0) gameRuntime.ReloadShaders();
            }
            window.PollEvents();
            if (window.ShouldClose()) {
                eyelib::api::core::Engine::RequestShutdown();
                return;
            }
        }

        gameRuntime.Tick(deltaSeconds);

        if (options.stats) {
            stats.accum += deltaSeconds;
            stats.frames += 1;
            if (stats.accum >= 1.0f) {
                eyelib::api::core::Log::InfoF("Stats: fps=%.1f entities=%zu",
                    eyelib::api::core::Time::FPS(), world.GetEntityCount());
                stats.accum = 0.0f;
                stats.frames = 0;
            }
        }

        if (!options.headless) {
            window.RefreshFramebufferSize();
            const int w = window.Width();
            const int h = window.Height();
            if (w > 0 && h > 0) {
                window.ApplyVsync();
                const uint32_t rw = static_cast<uint32_t>(w);
                const uint32_t rh = static_cast<uint32_t>(h);
                (void)gameRuntime.Render(rw, rh);
                if (options.overlay && overlay.IsReady()) {
                    OverlaySnapshot snap;
                    snap.fps = eyelib::api::core::Time::FPS();
                    snap.frameTimeMs = eyelib::api::core::Time::SmoothedDeltaTime() * 1000.0f;
                    snap.entityCount = world.GetEntityCount();
                    snap.scenePath = options.scenePath;
                    overlay.Render(snap);
                }
                window.SwapBuffers();
            }
        }
    };

    eyelib::api::core::Engine::Run(update, nullptr);

    if (!options.headless) {
        window.MakeContextCurrent();
    }
    gameRuntime.Shutdown();

    eyelib::api::core::Log::Info("Shutdown sequence completed");
    // Explicit engine shutdown so "Shutting down Eyelib Engine..." and "Log system shutting down" appear in order.
    // ScopedEngine destructor will no-op because Shutdown() sets s_initialized = false.
    eyelib::api::core::Engine::Shutdown();

    return static_cast<int>(ExitCode::Ok);
}

} // namespace openeye::player
