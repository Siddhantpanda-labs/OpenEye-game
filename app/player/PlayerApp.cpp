#include "PlayerApp.h"

#include "AssetBinding.h"
#include "Overlay.h"
#include "PlayerConfig.h"
#include "WorldRender.h"
#include "WorldPhysics.h"
#include "WorldAnimation.h"
#include "WorldScripting.h"

#include "api/assets/All.h"
#include "api/core/All.h"
#include "api/platform/All.h"
#include "api/world/All.h"

#include <functional>
#include <iostream>
#include <utility>

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
    return NormalizePath(GetAbsolutePath(path));
}

std::vector<std::string> NormalizeRoots(const std::vector<std::string>& roots) {
    std::vector<std::string> out;
    out.reserve(roots.size());
    for (const auto& root : roots) {
        out.push_back(NormalizePath(root));
    }
    return out;
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

    eyelib::api::AssetRegistry assetRegistry;
    const auto assetStats = BindAssets(world, options.assetRoots, assetRegistry);
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
        if (!renderBridge.Initialize(static_cast<uint32_t>(window.Width()),
                                     static_cast<uint32_t>(window.Height()))) {
            eyelib::api::core::Log::Error("Failed to initialize renderer or pipeline.");
            return static_cast<int>(ExitCode::RenderInitFailed);
        }
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
    scriptingBridge.BuildFromWorld(world, options.assetRoots);

    struct RuntimeStats {
        float accum = 0.0f;
        int frames = 0;
    } stats;

    auto update = [&world, &window, &options, &stats, &physicsBridge, &animationBridge, &scriptingBridge](float deltaSeconds) {
        if (!options.headless) {
            window.PollEvents();
            if (window.ShouldClose()) {
                eyelib::api::core::Engine::RequestShutdown();
                return;
            }
        }
        physicsBridge.Step(world, deltaSeconds, options.maxSubsteps,
                           options.fixedTimestep > 0.0f ? options.fixedTimestep : 0.0f);
        animationBridge.Update(deltaSeconds);
        scriptingBridge.Update(world, options.assetRoots);
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
    };

    std::function<void()> render;
    if (!options.headless) {
        render = [&window, &world, &options, &overlay, &renderBridge, &animationBridge, &assetRegistry]() {
            renderBridge.Render(options, world, &animationBridge, &assetRegistry,
                                static_cast<uint32_t>(window.Width()),
                                static_cast<uint32_t>(window.Height()));
            if (options.overlay && overlay.IsReady()) {
                overlay.Render(options, world);
            }
            window.SwapBuffers();
        };
    }

    eyelib::api::core::Engine::Run(update, render);

    renderBridge.Shutdown();
    physicsBridge.Shutdown();

    return static_cast<int>(ExitCode::Ok);
}

} // namespace openeye::player
