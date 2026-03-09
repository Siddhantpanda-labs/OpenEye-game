#include "EditorApp.h"

#include "EditorContext.h"
#include "EditorProject.h"
#include "EditorSceneBuilder.h"
#include "MenuBar.h"
#include "panels/StatusPanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/SceneViewPanel.h"
#include "panels/GameViewPanel.h"
#include "panels/AssetPanel.h"
#include "GameRuntime.h"

#include "api/assets/All.h"
#include "api/core/All.h"
#include "api/physics/PhysicsModule.h"
#include "api/platform/All.h"
#include "api/render/All.h"
#include "api/render/RenderDevice.h"
#include "api/platform/GraphicsDevice.h"
#include "api/render/RenderBackend.h"
#include "api/render/StaticResources.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstring>

namespace openeye::editor {

namespace {

class ScopedEngine {
public:
    bool Initialize(const eyelib::api::Engine::Config& config) {
        m_initialized = eyelib::api::Engine::Initialize(config);
        return m_initialized;
    }
    ~ScopedEngine() {
        if (m_initialized) {
            eyelib::api::Engine::Shutdown();
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

class ScopedImGui {
public:
    bool Initialize(eyelib::api::Window& window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();
        auto* glfwWindow = static_cast<GLFWwindow*>(window.GetNativeHandle());
        if (!ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true)) {
            ImGui::DestroyContext();
            return false;
        }
        if (!ImGui_ImplOpenGL3_Init("#version 450")) {
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            return false;
        }
        m_ready = true;
        return true;
    }
    ~ScopedImGui() {
        if (!m_ready) return;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
private:
    bool m_ready = false;
};

} // namespace

int RunEditor(int argc, char** argv) {
    const char* projectPathArg = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc) {
            projectPathArg = argv[i + 1];
            break;
        }
    }

    eyelib::api::Engine::Config engineConfig;
    engineConfig.enableLogging = true;
    engineConfig.logFile = "editor.log";
    engineConfig.headless = false;

    eyelib::api::Engine::RegisterSubsystem(std::make_unique<eyelib::api::PhysicsModule>());
    ScopedEngine engine;
    if (!engine.Initialize(engineConfig)) {
        std::fprintf(stderr, "Editor: Engine init failed.\n");
        return 1;
    }

    eyelib::api::Window window;
    eyelib::api::WindowConfig windowConfig;
    windowConfig.title = "OpenEye Editor";
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.vsync = true;
    windowConfig.resizable = true;

    ScopedWindow windowGuard;
    if (!windowGuard.Initialize(window, windowConfig)) {
        std::fprintf(stderr, "Editor: Window init failed.\n");
        return 2;
    }

    ScopedImGui imgui;
    if (!imgui.Initialize(window)) {
        std::fprintf(stderr, "Editor: ImGui init failed.\n");
        return 3;
    }

    // One RHI device per window; pipelines are view configs only (no pipeline-owned device).
    void* platformWindow = window.GetPlatformWindow();
    auto editorDevice = platformWindow
        ? eyelib::platform::CreateGraphicsDevice(
              static_cast<eyelib::platform::Window*>(platformWindow), 256, 256,
              eyelib::render::RenderBackend::OpenGL)
        : nullptr;
    eyelib::api::RenderDevice editorRenderDevice(editorDevice.get());

    eyelib::api::Renderer sceneViewRenderer;
    eyelib::api::RenderPipeline sceneViewPipeline;
    eyelib::api::RenderPipeline gameViewPipeline;
    if (!sceneViewRenderer.Initialize()) {
        std::fprintf(stderr, "Editor: Renderer init failed.\n");
    }
    if (!editorDevice) {
        std::fprintf(stderr, "Editor: CreateGraphicsDevice failed (scene/game view will show placeholder).\n");
    } else {
        if (!sceneViewPipeline.InitializeWithSharedDevice(editorRenderDevice, 256, 256)) {
            std::fprintf(stderr, "Editor: Scene view pipeline init failed (scene view will show placeholder).\n");
        } else {
            sceneViewPipeline.SetMode(eyelib::api::RenderPipeline::Mode::Forward);
        }
        if (!gameViewPipeline.InitializeWithSharedDevice(editorRenderDevice, 256, 256)) {
            std::fprintf(stderr, "Editor: Game view pipeline init failed (debug camera preview disabled).\n");
        } else {
            gameViewPipeline.SetMode(eyelib::api::RenderPipeline::Mode::Forward);
        }
    }

    // Engine assets (shaders, etc.) load directly from Engine/assets — no copy to exe dir
    std::string engineRoot = eyelib::api::platform::filesystem::GetEngineAssetsRoot();
    std::string engineAssets = "assets";
    if (!engineRoot.empty()) {
        engineAssets = eyelib::api::platform::filesystem::NormalizePath(engineRoot + "/assets");
        eyelib::api::platform::filesystem::SetWorkingDirectory(engineRoot);
    }
    eyelib::api::core::Log::InfoF("Editor engine assets: %s", engineAssets.c_str());

    eyelib::api::AssetRegistry assetRegistry;
    if (editorDevice)
        assetRegistry.SetRenderDevice(editorRenderDevice);

    openeye::editor::EditorContext ctx;
    ctx.sceneViewPipeline = &sceneViewPipeline;
    ctx.gameViewPipeline = &gameViewPipeline;
    ctx.assetRegistry = &assetRegistry;
    ctx.assetRoots = { engineAssets };
    ctx.engineAssetsPath = engineAssets;
    ctx.window = &window;

    if (projectPathArg && openeye::editor::OpenProject(ctx, projectPathArg)) {
        eyelib::api::core::Log::InfoF("Editor: opened project '%s' at %s", ctx.projectName.c_str(), ctx.projectPath.c_str());
    }

    if (editorRenderDevice) {
        auto defaultShader = std::make_shared<eyelib::api::Shader>();
        if (defaultShader->Load(editorRenderDevice, "passes/forward/lit")) {
            auto defaultMat = std::make_shared<eyelib::api::Material>();
            defaultMat->SetShader(defaultShader);
            ctx.defaultMaterial = std::move(defaultMat);
        }
    }

    while (!window.ShouldClose()) {
        window.PollEvents();
        window.MakeContextCurrent();
        window.RefreshFramebufferSize();
        if (ctx.gameRuntimeStopRequested && ctx.gameRuntime) {
            ctx.world.SetFrameSnapshot(nullptr);
            ctx.world.SetCaptureSnapshotCallback(nullptr);
            try {
                ctx.gameRuntime.reset();
            } catch (...) {}
            ctx.world.SetFrameSnapshot(nullptr);
            ctx.gameRuntimeStopRequested = false;
            // Model A: single device; no restore needed.
            // Revert world to pre-Play snapshot so edits during Play are discarded.
            if (!ctx.playSnapshotPath.empty()) {
                if (ctx.world.LoadScene(ctx.playSnapshotPath)) {
                    if (ctx.assetRegistry) {
                        EditorSceneBuilder::BindSceneAssets(ctx.world, ctx.assetRoots, *ctx.assetRegistry);
                    }
                    ctx.selectedEntity = ::eyelib::world::InvalidEntity;
                    ctx.sceneSelected = false;
                }
                (void)eyelib::api::platform::filesystem::DeleteFile(ctx.playSnapshotPath);
                ctx.playSnapshotPath.clear();
            }
            eyelib::api::core::Log::Info("Game runtime stopped; default device restored.");
        }
        window.Clear(0.2f, 0.2f, 0.25f, 1.0f);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        openeye::editor::ShowMenuBar(ctx);

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        openeye::editor::panels::ShowStatusPanel(&ctx);
        openeye::editor::panels::ShowHierarchyPanel(&ctx);
        openeye::editor::panels::ShowInspectorPanel(&ctx);
        openeye::editor::panels::ShowAssetPanel(&ctx);
        openeye::editor::panels::ShowSceneViewPanel(&ctx);
        openeye::editor::panels::ShowGameViewPanel(&ctx);

        ImGui::Render();
        // Restore default framebuffer and full-window viewport so ImGui draws to the window.
        // The scene view and game view pipelines leave their FBO bound after Render().
        if (ctx.sceneViewPipeline)
            ctx.sceneViewPipeline->RestoreDefaultFramebufferAndViewport(
                static_cast<uint32_t>(window.Width()), static_cast<uint32_t>(window.Height()));
        if (ctx.gameViewPipeline)
            ctx.gameViewPipeline->RestoreDefaultFramebufferAndViewport(
                static_cast<uint32_t>(window.Width()), static_cast<uint32_t>(window.Height()));
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        window.SwapBuffers();
    }

    // Teardown: pipelines release FBOs; then static GPU resources; device is destroyed when editorDevice goes out of scope.
    sceneViewPipeline.Shutdown();
    gameViewPipeline.Shutdown();
    if (editorDevice) {
        eyelib::render::DebugDrawRenderer::Shutdown();
        eyelib::render::Primitives::ShutdownStaticResources();
    }

    return 0;
}

} // namespace openeye::editor
