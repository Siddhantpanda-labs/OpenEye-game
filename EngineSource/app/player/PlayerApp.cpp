#include "PlayerApp.h"

#include "GameRuntime.h"
#include "Overlay.h"
#include "PlayerConfig.h"

#include "api/core/All.h"
#include "api/physics/PhysicsModule.h"
#include "api/platform/All.h"
#include "api/world/All.h"
#include "api/render/RenderBackend.h"
#include "api/rendering/RenderPipeline.h"  // M4.7 — RuntimeOverlay::Initialize(window, pipeline) overload.

#ifdef OPENEYE_BUILD_STREAMING
#include "app/streaming/VRStreamingServer.h"
#endif

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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

    if (!ApplyRuntimeProfile(options, error)) {
        if (!error.empty()) {
            std::cerr << error << "\n";
        }
        return static_cast<int>(ExitCode::InvalidArgs);
    }

    ApplyRuntimeDefaults(options);
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
        windowConfig.useNativeApiOnly =
            (options.renderBackend == eyelib::render::RenderBackend::Vulkan ||
             options.renderBackend == eyelib::render::RenderBackend::Diligent);
        if (!windowGuard.Initialize(window, windowConfig)) {
            eyelib::api::core::Log::Error("Failed to initialize window.");
            return static_cast<int>(ExitCode::WindowInitFailed);
        }
    }

    RuntimeOverlay overlay;
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

    if (!options.headless && options.overlay) {
        // M4.7 — pick the new-API path when GameRuntime is on the new pipeline; fall back to
        // the legacy void*-device path otherwise. The legacy path itself routes to
        // OpenGL / legacy-Vulkan internally based on `options.renderBackend`.
        if (auto* pipelineNew = gameRuntime.GetRenderPipelineNew()) {
            overlay.Initialize(window, *pipelineNew);
        } else {
            overlay.Initialize(window, options.renderBackend, gameRuntime.GetDevice().Get());
        }
    }
    ScopedOverlay overlayGuard(overlay);

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

#ifdef OPENEYE_BUILD_STREAMING
    gameRuntime.SetStreamingGyroCallback([&world](float yaw, float pitch, float roll) {
        auto eid = world.GetFirstXRCameraEntityId();
        if (!eid) return;
        eyelib::world::components::Transform t;
        if (!world.GetWorldTransform(*eid, t)) return;
        glm::mat4 R = glm::rotate(glm::mat4(1.f), yaw, glm::vec3(0.f, 1.f, 0.f))
                    * glm::rotate(glm::mat4(1.f), pitch, glm::vec3(1.f, 0.f, 0.f))
                    * glm::rotate(glm::mat4(1.f), roll, glm::vec3(0.f, 0.f, 1.f));
        t.rotation = glm::quat_cast(R);
        world.SetTransform(*eid, t);
    });
    if (!options.headless && gameRuntime.StartStreaming(27016)) {
        eyelib::api::core::Log::Info("VR streaming server started (discovery 27015, stream 27016).");
    }
#endif

    // Shaders and engine assets load directly from Engine/assets — no ShaderWatcher copy to exe dir
    if (!options.headless) {
        std::string engineRoot = eyelib::api::platform::filesystem::GetEngineAssetsRoot();
        if (!engineRoot.empty()) {
            eyelib::api::platform::filesystem::SetWorkingDirectory(engineRoot);
            eyelib::api::core::Log::InfoF("Player engine assets: %s", eyelib::api::platform::filesystem::NormalizePath(engineRoot + "/assets").c_str());
        }
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
                OverlaySnapshot snap;
                if (options.overlay && overlay.IsReady()) {
                    snap.fps = eyelib::api::core::Time::FPS();
                    snap.frameTimeMs = eyelib::api::core::Time::SmoothedDeltaTime() * 1000.0f;
                    snap.entityCount = world.GetEntityCount();
                    snap.scenePath = options.scenePath;
#ifdef OPENEYE_BUILD_STREAMING
                    if (gameRuntime.IsStreaming()) {
                        snap.streamingActive = true;
                        snap.streamingClientCount = gameRuntime.GetStreamingClientCount();
                        snap.streamingFps = gameRuntime.GetStreamingFPS();
                        std::string ip = eyelib::app::VRStreamingServer::GetLocalIPv4Address();
                        if (!ip.empty())
                            snap.streamingServerAddress = ip + ":27016";
                    }
#endif
                }
                gameRuntime.BeginPresentFrame();
                if (overlay.IsVulkan() && options.overlay && overlay.IsReady()) {
                    overlay.Render(snap);
                }
                (void)gameRuntime.Render(rw, rh);
                if (!overlay.IsVulkan() && options.overlay && overlay.IsReady()) {
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
