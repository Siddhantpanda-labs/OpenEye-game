#include "AIPlaygroundApp.h"
#include "AIPlaygroundUI.h"
#include "PlaygroundGeminiCoordinator.h"
#include "PlaygroundOpenAiCoordinator.h"
#include "PlaygroundProject.h"
#include "WorldToolBridge.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "../player/GameRuntime.h"
#include "../player/PlayerConfig.h"

#include "api/core/All.h"
#include "api/physics/PhysicsModule.h"
#include "api/platform/All.h"
#include "api/render/RenderBackend.h"
#include "api/render/CameraConventions.h"
#include "eyelib/math/Types.h"
#include "api/world/All.h"
#include "eyelib/render/rhi/RHI.h"

#include <imgui.h>

#include <cstdio>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef DeleteFile
#endif

namespace openeye::ai_playground {

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

class ScopedUI {
public:
    explicit ScopedUI(AIPlaygroundUI& ui) : m_ui(&ui) {}

    ~ScopedUI() {
        if (m_ui) {
            m_ui->Shutdown();
        }
    }

private:
    AIPlaygroundUI* m_ui = nullptr;
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
    for (const std::string& root : roots) {
        out.push_back(NormalizePath(root));
    }
    return out;
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

static bool IsAbsolutePath(const std::string& path) {
    if (path.empty()) {
        return false;
    }
#ifdef _WIN32
    if (path.size() > 1 && path[1] == ':') {
        return true;
    }
#endif
    return path[0] == '/' || path[0] == '\\';
}

static void ResolveLogPathToExeDir(openeye::player::RuntimeOptions& options) {
    using namespace eyelib::api::platform::filesystem;
    std::string exePath = GetExecutablePath();
    if (exePath.empty()) {
        return;
    }
    std::string exeDir = GetDirectory(exePath);
    if (exeDir.empty()) {
        return;
    }
    if (!IsAbsolutePath(options.logPath)) {
        if (!exeDir.empty() && exeDir.back() != '/' && exeDir.back() != '\\') {
            exeDir += "/";
        }
        std::string full = GetAbsolutePath(exeDir + options.logPath);
        options.logPath = eyelib::api::platform::filesystem::NormalizePath(full);
    }
}

static bool HasExplicitSceneArgument(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--scene" && i + 1 < argc) {
            return true;
        }
    }
    return false;
}

static bool ReloadPlaygroundRuntime(AIPlaygroundUI& ui,
                                    eyelib::api::Window& window,
                                    const std::string& engineAssetsNormalized,
                                    eyelib::api::world::World& world,
                                    eyelib::api::AssetRegistry& reg,
                                    openeye::player::GameRuntime& rt,
                                    const std::string& projectRootAbs,
                                    const std::string& scenePathAbs,
                                    openeye::player::RuntimeOptions& opt,
                                    uint32_t w,
                                    uint32_t h,
                                    eyelib::render::RenderBackend backend,
                                    std::string& err) {
    // Idle GPU before ImGui/pipeline teardown so Vulkan does not destroy resources still referenced by in-flight command buffers.
    if (rt.IsReady()) {
        if (void* dev = rt.GetDevice().Get()) {
            static_cast<eyelib::render::rhi::IDevice*>(dev)->Finish();
        }
    }
    ui.Shutdown();
    rt.Shutdown();
    reg.Clear();
    if (!world.LoadScene(scenePathAbs)) {
        err = "LoadScene failed: " + scenePathAbs;
        eyelib::api::core::Log::ErrorF("AI Playground reload failed: %s", err.c_str());
        return false;
    }
    world.FlushTransforms();

    std::vector<std::string> roots;
    roots.push_back(engineAssetsNormalized);
    if (!projectRootAbs.empty()) {
        roots.push_back(eyelib::api::platform::filesystem::NormalizePath(projectRootAbs + "/Assets"));
    }
    opt.assetRoots = NormalizeRoots(roots);

    if (!rt.Initialize(world, reg, opt.assetRoots, &window, w, h, backend, eyelib::api::RenderDevice(), &opt)) {
        err = "GameRuntime::Initialize failed.";
        eyelib::api::core::Log::ErrorF("AI Playground reload failed: %s", err.c_str());
        return false;
    }
    if (!ui.Initialize(window, backend, rt.GetDevice().Get())) {
        err = "ImGui re-initialize failed after project load.";
        eyelib::api::core::Log::ErrorF("AI Playground reload failed: %s", err.c_str());
        return false;
    }
    return true;
}

/** Free-fly spectator (edit / preview only): same basis as editor scene view. */
constexpr float kSpectatorPitchMin = -1.55f;
constexpr float kSpectatorPitchMax = 1.55f;

struct PlaygroundSpectatorCamera {
    float position[3] = {0.0f, 0.0f, 10.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fovDegrees = 60.0f;
    float moveSpeed = 8.0f;
    float mouseSensitivity = 0.002f;
};

static std::string PlaySessionSnapshotAbsolutePath() {
    using namespace eyelib::api::platform::filesystem;
    const std::string exePath = GetExecutablePath();
    std::string dir = GetDirectory(exePath);
    if (dir.empty()) {
        return NormalizePath(std::string(".openeye_ai_playground_play.json"));
    }
    if (dir.back() != '/' && dir.back() != '\\') {
        dir += '/';
    }
    return NormalizePath(dir + ".openeye_ai_playground_play.json");
}

/** ImGui may claim keyboard for nav; never steal keys while typing in an input field. */
static bool SpectatorMovementKeysAllowed(const ImGuiIO& io) {
    if (io.WantTextInput) {
        return false;
    }
    if (!io.WantCaptureKeyboard) {
        return true;
    }
    return ImGui::IsMouseDown(ImGuiMouseButton_Right);
}

static void ApplyPlaygroundSpectatorInput(PlaygroundSpectatorCamera& cam, float deltaSeconds, bool simulationRunning) {
    if (simulationRunning) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    const float dt = deltaSeconds > 0.f ? deltaSeconds : (io.DeltaTime > 0.f ? io.DeltaTime : 0.016f);

    if (!io.WantCaptureMouse && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        cam.yaw += io.MouseDelta.x * cam.mouseSensitivity;
        cam.pitch -= io.MouseDelta.y * cam.mouseSensitivity;
        if (cam.pitch > kSpectatorPitchMax) {
            cam.pitch = kSpectatorPitchMax;
        }
        if (cam.pitch < kSpectatorPitchMin) {
            cam.pitch = kSpectatorPitchMin;
        }
    }

    if (!SpectatorMovementKeysAllowed(io)) {
        return;
    }

    const float cy = std::cos(cam.pitch);
    const float sy = std::sin(cam.pitch);
    const float ct = std::cos(cam.yaw);
    const float st = std::sin(cam.yaw);
    const float forwardX = cy * st;
    const float forwardY = sy;
    const float forwardZ = -cy * ct;
    float rx = cy * ct;
    float rz = cy * st;
    const float rlen = std::sqrt(rx * rx + rz * rz);
    if (rlen > 1e-5f) {
        rx /= rlen;
        rz /= rlen;
    }
    const float move = cam.moveSpeed * dt;
    if (ImGui::IsKeyDown(ImGuiKey_W)) {
        cam.position[0] += forwardX * move;
        cam.position[1] += forwardY * move;
        cam.position[2] += forwardZ * move;
    }
    if (ImGui::IsKeyDown(ImGuiKey_S)) {
        cam.position[0] -= forwardX * move;
        cam.position[1] -= forwardY * move;
        cam.position[2] -= forwardZ * move;
    }
    if (ImGui::IsKeyDown(ImGuiKey_A)) {
        cam.position[0] -= rx * move;
        cam.position[2] -= rz * move;
    }
    if (ImGui::IsKeyDown(ImGuiKey_D)) {
        cam.position[0] += rx * move;
        cam.position[2] += rz * move;
    }
    if (ImGui::IsKeyDown(ImGuiKey_Q)) {
        cam.position[1] -= move;
    }
    if (ImGui::IsKeyDown(ImGuiKey_E)) {
        cam.position[1] += move;
    }
}

} // namespace

int RunAIPlayground(int argc, char** argv) {
    AttachConsoleIfNeeded();

    const bool userExplicitScene = HasExplicitSceneArgument(argc, argv);

    openeye::player::RuntimeOptions options;
    options.title = "OpenEye AI Playground";
    options.logPath = "OpenEyeAIPlayground.log";
    options.overlay = true;
    options.overlaySet = true;

    std::string error;
    const std::string configPath = openeye::player::FindConfigPath(argc, argv);
    if (!configPath.empty()) {
        if (!openeye::player::LoadConfig(configPath, options, error)) {
            if (!error.empty()) {
                std::cerr << error << "\n";
            }
            return static_cast<int>(ExitCode::ConfigError);
        }
    }
    if (!openeye::player::ParseArgs(argc, argv, options, error)) {
        if (!error.empty()) {
            std::cerr << error << "\n";
        }
        openeye::player::PrintUsage(argv[0]);
        return static_cast<int>(ExitCode::InvalidArgs);
    }
    if (options.showHelp) {
        openeye::player::PrintUsage(argv[0]);
        return static_cast<int>(ExitCode::Ok);
    }

    if (!userExplicitScene) {
        options.scenePath.clear();
    }

    if (!openeye::player::ApplyRuntimeProfile(options, error)) {
        if (!error.empty()) {
            std::cerr << error << "\n";
        }
        return static_cast<int>(ExitCode::InvalidArgs);
    }

    openeye::player::ApplyRuntimeDefaults(options);
    if (options.renderBackend == eyelib::render::RenderBackend::Default) {
        options.renderBackend = eyelib::render::RenderBackend::Vulkan;
    }
    if (!options.scenePath.empty()) {
        options.scenePath = NormalizePath(options.scenePath);
    }
    options.assetRoots = NormalizeRoots(options.assetRoots);
    ResolveLogPathToExeDir(options);

    if (options.headless) {
        if (!userExplicitScene || options.scenePath.empty()) {
            std::cerr << "Headless AI Playground requires --scene <path>.\n";
            return static_cast<int>(ExitCode::InvalidArgs);
        }
        if (!eyelib::api::platform::filesystem::FileExists(options.scenePath)) {
            std::cerr << "Scene file not found: " << options.scenePath << "\n";
            return static_cast<int>(ExitCode::SceneMissing);
        }
    }

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

    eyelib::api::core::Log::Info("OpenEye AI Playground starting.");
    if (userExplicitScene) {
        eyelib::api::core::Log::InfoF("CLI scene: %s", options.scenePath.c_str());
    } else {
        eyelib::api::core::Log::Info("No initial scene — use New/Open Playground (Projects/AI_Playground/<name>).");
    }

    if (userExplicitScene && !eyelib::api::platform::filesystem::FileExists(options.scenePath)) {
        eyelib::api::core::Log::ErrorF("Scene file not found: %s", options.scenePath.c_str());
        return static_cast<int>(ExitCode::SceneMissing);
    }

    eyelib::api::world::World world;
    if (options.fixedTimestep > 0.0f) {
        world.SetFixedTimestep(options.fixedTimestep);
        if (!options.timeModeSet) {
            options.timeMode = openeye::player::TimeMode::Fixed;
            options.timeModeSet = true;
        }
    }
    world.SetMaxSubsteps(options.maxSubsteps);
    world.SetTimeScale(options.timeScale);
    if (options.timeModeSet) {
        ::eyelib::world::WorldScheduler::TimeMode mode = ::eyelib::world::WorldScheduler::TimeMode::Realtime;
        if (options.timeMode == openeye::player::TimeMode::Fixed) {
            mode = ::eyelib::world::WorldScheduler::TimeMode::Fixed;
        } else if (options.timeMode == openeye::player::TimeMode::Replay) {
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

    if (userExplicitScene) {
        if (!world.LoadScene(options.scenePath)) {
            eyelib::api::core::Log::ErrorF("Failed to load scene: %s", options.scenePath.c_str());
            return static_cast<int>(ExitCode::SceneLoadFailed);
        }
    }

    eyelib::api::Window window;
    ScopedWindow windowGuard;
    if (!options.headless) {
        eyelib::api::WindowConfig windowConfig;
        windowConfig.title = options.title;
        windowConfig.width = options.width;
        windowConfig.height = options.height;
        windowConfig.vsync = options.vsync;
        // Vulkan (and Diligent) require GLFW_NO_API — an OpenGL context prevents glfwCreateWindowSurface / ImGui Vulkan.
        windowConfig.useNativeApiOnly =
            (options.renderBackend == eyelib::render::RenderBackend::Vulkan ||
             options.renderBackend == eyelib::render::RenderBackend::Diligent);
        if (!windowGuard.Initialize(window, windowConfig)) {
            eyelib::api::core::Log::Error("Failed to initialize window.");
            return static_cast<int>(ExitCode::WindowInitFailed);
        }
    }

    AIPlaygroundUI ui;
    AIPlaygroundChatState chat;
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

    if (!options.headless) {
        if (!ui.Initialize(window, options.renderBackend, gameRuntime.GetDevice().Get())) {
            eyelib::api::core::Log::Error("Failed to initialize AI Playground UI (ImGui).");
            return static_cast<int>(ExitCode::RenderInitFailed);
        }
    }
    ScopedUI uiGuard(ui);

    const auto assetStats = gameRuntime.GetAssetBindingStats();
    if (options.strictAssets && (assetStats.missing > 0 || assetStats.unknownType > 0)) {
        eyelib::api::core::Log::Error("Strict asset mode: missing or unknown assets detected.");
        return static_cast<int>(ExitCode::AssetStrictFailed);
    }

    std::string engineRootForCwd;
    if (!options.headless) {
        engineRootForCwd = eyelib::api::platform::filesystem::GetEngineAssetsRoot();
        if (!engineRootForCwd.empty()) {
            eyelib::api::platform::filesystem::SetWorkingDirectory(engineRootForCwd);
            eyelib::api::core::Log::InfoF("Engine root: %s",
                eyelib::api::platform::filesystem::NormalizePath(engineRootForCwd).c_str());
        }
    }

    const std::string workspaceRoot = FindOpenEyeWorkspaceRoot();
    std::string engineAssetsNormalized;
    if (!engineRootForCwd.empty()) {
        engineAssetsNormalized = NormalizePath(engineRootForCwd + "/assets");
    }

    struct PlaygroundSession {
        std::string playgroundName;
        std::string rootAbs;
        std::string sceneAbs;
        std::string displayScene;
        bool cliScene = false;
    } session;
    PlaygroundSpectatorCamera spectatorCam;
    bool simulationRunning = false;
    std::string playSessionSnapshotPath;

    if (userExplicitScene) {
        session.cliScene = true;
        session.playgroundName = "(CLI)";
        session.sceneAbs = options.scenePath;
        session.displayScene = options.scenePath;
    }

    struct PendingPlayground {
        enum class Kind { None, NewName, OpenName } kind = Kind::None;
        std::string name;
        bool newUsePackagedTemplate = false;
    } pending;

    std::string lastProjectError;

    const auto discardPlaySessionArtifact = [&]() {
        simulationRunning = false;
        if (playSessionSnapshotPath.empty()) {
            return;
        }
        using namespace eyelib::api::platform::filesystem;
        if (FileExists(playSessionSnapshotPath)) {
            (void)DeleteFile(playSessionSnapshotPath);
        }
        playSessionSnapshotPath.clear();
    };

    const auto stopSimulation = [&]() {
        if (!simulationRunning) {
            return;
        }
        simulationRunning = false;
        gameRuntime.SetFrameCameraOverride(nullptr);
        const std::string snap = playSessionSnapshotPath;
        playSessionSnapshotPath.clear();
        if (!snap.empty()) {
            using namespace eyelib::api::platform::filesystem;
            if (!world.LoadScene(snap)) {
                lastProjectError = "Stop: failed to restore session snapshot.";
                eyelib::api::core::Log::ErrorF("AI Playground Stop: LoadScene failed for %s", snap.c_str());
                playSessionSnapshotPath = snap;
            } else if (FileExists(snap)) {
                (void)DeleteFile(snap);
            }
        }
        world.FlushTransforms();
        gameRuntime.ResyncFromWorld();
    };

    const auto startSimulation = [&]() {
        if (simulationRunning || !gameRuntime.IsReady()) {
            return;
        }
        if (session.sceneAbs.empty() && !userExplicitScene) {
            lastProjectError = "Load a playground or pass --scene before Play.";
            return;
        }
        const std::string path = PlaySessionSnapshotAbsolutePath();
        using namespace eyelib::api::platform::filesystem;
        if (FileExists(path)) {
            (void)DeleteFile(path);
        }
        if (!world.SaveScene(path)) {
            lastProjectError = "Play: could not write session snapshot.";
            eyelib::api::core::Log::ErrorF("AI Playground Play: SaveScene failed for %s", path.c_str());
            return;
        }
        playSessionSnapshotPath = path;
        simulationRunning = true;
        gameRuntime.SetFrameCameraOverride(nullptr);
    };

    PlaygroundUIMenu playgroundMenu;
    playgroundMenu.lastError = &lastProjectError;
    playgroundMenu.onNew = [&pending](const std::string& raw, bool loadPackagedTemplate) {
        pending.kind = PendingPlayground::Kind::NewName;
        pending.name = raw;
        pending.newUsePackagedTemplate = loadPackagedTemplate;
    };
    playgroundMenu.onOpen = [&pending](const std::string& pick) {
        pending.kind = PendingPlayground::Kind::OpenName;
        pending.name = pick;
    };
    playgroundMenu.onSimulationPlay = [startSimulation]() { startSimulation(); };
    playgroundMenu.onSimulationStop = [stopSimulation]() { stopSimulation(); };
    playgroundMenu.onSaveScene = [&]() {
        if (!gameRuntime.IsReady()) {
            lastProjectError = "Save: runtime not ready.";
            return;
        }
        std::string path;
        if (!session.sceneAbs.empty()) {
            path = session.sceneAbs;
        } else if (userExplicitScene && !options.scenePath.empty()) {
            path = options.scenePath;
        }
        if (path.empty()) {
            lastProjectError = "Save: open a playground or pass --scene.";
            return;
        }
        lastProjectError.clear();
        if (!world.SaveScene(path)) {
            lastProjectError = "Save failed: " + path;
            eyelib::api::core::Log::ErrorF("AI Playground Save: SaveScene failed for %s", path.c_str());
            return;
        }
        chat.messages.push_back("system: Scene saved.");
        eyelib::api::core::Log::InfoF("AI Playground: scene saved to %s", path.c_str());
    };

    WorldToolBridge toolBridge(world);
    OpenAiAskAsyncCoordinator openAiCoordinator;
    GeminiAskAsyncCoordinator geminiCoordinator;

    chat.messages.push_back(
        "system: Type /help for commands. Phase 1: /observe, /plan, /ai demo; `/ask <goal>` uses the **LLM for /ask** combo "
        "(ChatGPT needs OPENAI_API_KEY; Gemini needs GEMINI_API_KEY or **gemini.key.txt** at workspace root). "
        "Catalog: documentation/Ai_plan/tools_catalog_v1.md. **New/Open Playground** → Projects/AI_Playground/<name>.");

    ChatSubmitHandler onChat = [&](AIPlaygroundChatState& chatState, const std::string& userLine) {
        auto trimChat = [](std::string s) {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
                s.erase(s.begin());
            }
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
                s.pop_back();
            }
            return s;
        };
        const std::string t = trimChat(userLine);
        static const std::string kAsk = "/ask ";
        if (t.size() >= kAsk.size() && t.compare(0, kAsk.size(), kAsk) == 0) {
            const std::string goal = trimChat(t.substr(kAsk.size()));
            if (openAiCoordinator.IsBusy() || geminiCoordinator.IsBusy()) {
                chatState.messages.push_back("assistant: Wait for the current LLM request to finish.");
                return;
            }
            if (chatState.llmProviderIndex == 1) {
                (void)geminiCoordinator.TryStart(goal, workspaceRoot, chatState.messages);
            } else {
                (void)openAiCoordinator.TryStart(goal, workspaceRoot, chatState.messages);
            }
            return;
        }

        std::vector<std::string> replies;
        toolBridge.HandleUserLine(userLine, replies);
        for (std::string& r : replies) {
            chatState.messages.push_back(std::move(r));
        }
        if (gameRuntime.IsReady()) {
            gameRuntime.ResyncFromWorld();
        }
    };

    eyelib::api::platform::time::FramePacer framePacer;
    if (!options.headless && !options.vsync) {
        framePacer.SetTargetFPS(60.0);
    }

    int frameCount = 0;
    auto update = [&](float deltaSeconds) {
        if (!options.headless && !options.vsync) {
            framePacer.WaitForNextFrame();
        }
        world.SetFrameSnapshot(nullptr);
        if (gameRuntime.IsReady()) {
            const auto resync = [&]() {
                if (gameRuntime.IsReady()) {
                    gameRuntime.ResyncFromWorld();
                }
            };
            openAiCoordinator.Poll(toolBridge, chat.messages, resync);
            geminiCoordinator.Poll(toolBridge, chat.messages, resync);
        }
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

        if (!options.headless && pending.kind != PendingPlayground::Kind::None) {
            playgroundMenu.existingNames = ListPlaygroundProjects(workspaceRoot);
            lastProjectError.clear();
            window.RefreshFramebufferSize();
            const int ww = (std::max)(1, window.Width());
            const int hh = (std::max)(1, window.Height());
            const uint32_t rw = static_cast<uint32_t>(ww);
            const uint32_t rh = static_cast<uint32_t>(hh);

            if (workspaceRoot.empty()) {
                lastProjectError = "Workspace root not found (need EngineSource + Projects next to exe tree).";
            } else if (engineAssetsNormalized.empty()) {
                lastProjectError = "Engine assets folder not found.";
            } else if (pending.kind == PendingPlayground::Kind::NewName) {
                discardPlaySessionArtifact();
                const std::string cErr = CreatePlaygroundProject(workspaceRoot, pending.name,
                    pending.newUsePackagedTemplate, engineAssetsNormalized);
                if (!cErr.empty()) {
                    lastProjectError = cErr;
                } else {
                    const std::string sanitized = SanitizePlaygroundName(pending.name);
                    session.playgroundName = sanitized;
                    session.rootAbs = PlaygroundProjectRoot(workspaceRoot, sanitized);
                    PlaygroundManifest man;
                    const std::string mErr = ReadPlaygroundManifest(session.rootAbs, man);
                    if (!mErr.empty()) {
                        lastProjectError = mErr;
                    } else {
                        session.sceneAbs = ResolveScenePath(session.rootAbs, man);
                        session.displayScene =
                            std::string(kPlaygroundParentDir) + "/" + sanitized + "/" + man.sceneRelative;
                        if (!ReloadPlaygroundRuntime(ui, window, engineAssetsNormalized, world, assetRegistry,
                                gameRuntime, session.rootAbs, session.sceneAbs, options, rw, rh,
                                options.renderBackend, lastProjectError)) {
                            // lastProjectError set by Reload
                        } else {
                            chat.messages.push_back("system: Created playground \"" + sanitized + "\".");
                        }
                    }
                }
            } else if (pending.kind == PendingPlayground::Kind::OpenName) {
                discardPlaySessionArtifact();
                session.playgroundName = pending.name;
                session.rootAbs = PlaygroundProjectRoot(workspaceRoot, pending.name);
                PlaygroundManifest man;
                const std::string mErr = ReadPlaygroundManifest(session.rootAbs, man);
                if (!mErr.empty()) {
                    lastProjectError = mErr;
                } else {
                    session.sceneAbs = ResolveScenePath(session.rootAbs, man);
                    session.displayScene =
                        std::string(kPlaygroundParentDir) + "/" + pending.name + "/" + man.sceneRelative;
                    if (!eyelib::api::platform::filesystem::FileExists(session.sceneAbs)) {
                        lastProjectError = "Scene missing: " + session.sceneAbs;
                    } else if (!ReloadPlaygroundRuntime(ui, window, engineAssetsNormalized, world, assetRegistry,
                                 gameRuntime, session.rootAbs, session.sceneAbs, options, rw, rh,
                                 options.renderBackend, lastProjectError)) {
                    } else {
                        chat.messages.push_back("system: Opened playground \"" + pending.name + "\".");
                    }
                }
            }
            pending.kind = PendingPlayground::Kind::None;
            pending.name.clear();
            pending.newUsePackagedTemplate = false;
        }

        if (options.headless) {
            if (gameRuntime.IsReady()) {
                gameRuntime.Tick(deltaSeconds);
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

                if (gameRuntime.IsReady() && ui.IsReady()) {
                    if (simulationRunning) {
                        gameRuntime.SetFrameCameraOverride(nullptr);
                    } else {
                        ApplyPlaygroundSpectatorInput(spectatorCam, deltaSeconds, simulationRunning);
                        const uint32_t cw = (std::max)(1u, rw);
                        const uint32_t ch = (std::max)(1u, rh);
                        eyelib::api::camera::FreeFlyCameraDesc cameraDesc;
                        cameraDesc.position =
                            eyelib::math::Vec3(spectatorCam.position[0], spectatorCam.position[1], spectatorCam.position[2]);
                        cameraDesc.yaw = spectatorCam.yaw;
                        cameraDesc.pitch = spectatorCam.pitch;
                        cameraDesc.fovDegrees = spectatorCam.fovDegrees;
                        cameraDesc.nearPlane = 0.1f;
                        cameraDesc.farPlane = 1000.0f;
                        eyelib::api::CameraView freeFly =
                            eyelib::api::camera::BuildFreeFlyCameraView(cameraDesc, static_cast<float>(cw), static_cast<float>(ch));
                        gameRuntime.SetFrameCameraOverride(&freeFly);
                    }
                }

                playgroundMenu.existingNames = ListPlaygroundProjects(workspaceRoot);

                AIPlaygroundSnapshot snap;
                snap.fps = eyelib::api::core::Time::FPS();
                snap.frameTimeMs = eyelib::api::core::Time::SmoothedDeltaTime() * 1000.0f;
                snap.entityCount = world.GetEntityCount();
                snap.workspaceFound = !workspaceRoot.empty();
                snap.projectName = session.playgroundName;
                snap.simulationRunning = simulationRunning;
                snap.simulationCanStart =
                    gameRuntime.IsReady() && (!session.sceneAbs.empty() || userExplicitScene);
                if (!session.rootAbs.empty()) {
                    snap.projectPathRelative = std::string(kPlaygroundParentDir) + "/" + session.playgroundName;
                }
                if (!session.displayScene.empty()) {
                    snap.scenePath = session.displayScene;
                } else if (userExplicitScene) {
                    snap.scenePath = options.scenePath;
                } else {
                    snap.scenePath = "(no scene — use New/Open Playground)";
                }
                {
                    const char* k = std::getenv("OPENAI_API_KEY");
                    if (k != nullptr && k[0] != '\0') {
                        snap.llmStatus = "OpenAI: /ask (OPENAI_MODEL optional)";
                    } else {
                        snap.llmStatus = "Local: /observe /plan /ai demo (set OPENAI_API_KEY for /ask)";
                    }
                }

                gameRuntime.BeginPresentFrame();
                if (ui.IsVulkan() && ui.IsReady()) {
                    ui.Render(snap, chat, onChat, &playgroundMenu);
                }
                if (gameRuntime.IsReady() && !simulationRunning) {
                    gameRuntime.RefreshRenderSnapshotFromWorld();
                }
                (void)gameRuntime.Render(rw, rh);
                if (!ui.IsVulkan() && ui.IsReady()) {
                    ui.Render(snap, chat, onChat, &playgroundMenu);
                }
                if (gameRuntime.IsReady() && (simulationRunning || options.maxFrames > 0)) {
                    gameRuntime.Tick(deltaSeconds);
                }
                window.SwapBuffers();
            }
        }
    };

    eyelib::api::core::Engine::Run(update, nullptr);

    if (!options.headless) {
        window.MakeContextCurrent();
    }
    openAiCoordinator.ShutdownJoin();
    geminiCoordinator.ShutdownJoin();
    gameRuntime.Shutdown();

    eyelib::api::core::Log::Info("AI Playground shutdown complete.");
    eyelib::api::core::Engine::Shutdown();

    return static_cast<int>(ExitCode::Ok);
}

} // namespace openeye::ai_playground
