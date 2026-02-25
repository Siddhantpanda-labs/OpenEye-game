#include "EditorApp.h"

#include "EditorContext.h"
#include "EditorSceneBuilder.h"
#include "MenuBar.h"
#include "panels/StatusPanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/SceneViewPanel.h"
#include "panels/GameViewPanel.h"
#include "GameRuntime.h"

#include "api/assets/All.h"
#include "api/core/All.h"
#include "api/platform/All.h"
#include "api/render/All.h"
#include "eyelib/assets/ShaderWatcher.h"
#include "eyelib/render/RenderBackend.h"
#include "eyelib/render/rhi/RHIContext.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <GLFW/glfw3.h>

#include <cstdio>

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
    (void)argc;
    (void)argv;

    eyelib::api::Engine::Config engineConfig;
    engineConfig.enableLogging = true;
    engineConfig.logFile = "editor.log";
    engineConfig.headless = false;

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

    eyelib::api::Renderer sceneViewRenderer;
    eyelib::api::RenderPipeline sceneViewPipeline;
    if (!sceneViewRenderer.Initialize() || !sceneViewPipeline.Initialize(window, 256, 256, eyelib::render::RenderBackend::OpenGL)) {
        std::fprintf(stderr, "Editor: Scene view pipeline init failed (scene view will show placeholder).\n");
    } else {
        sceneViewPipeline.SetMode(eyelib::api::RenderPipeline::Mode::Forward);
    }

    std::unique_ptr<eyelib::assets::ShaderWatcher> shaderWatcher = std::make_unique<eyelib::assets::ShaderWatcher>();
    {
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
        eyelib::api::core::Log::InfoF("Editor ShaderWatcher source: %s", sourceAssets.c_str());
        eyelib::api::core::Log::InfoF("Editor ShaderWatcher runtime: %s", runtimeAssets.c_str());
    }

    eyelib::api::AssetRegistry assetRegistry;

    openeye::editor::EditorContext ctx;
    ctx.sceneViewPipeline = &sceneViewPipeline;
    ctx.assetRegistry = &assetRegistry;
    ctx.assetRoots = { "assets" };
    ctx.window = &window;

    while (!window.ShouldClose()) {
        window.PollEvents();
        window.MakeContextCurrent();
        window.RefreshFramebufferSize();
        if (ctx.gameRuntimeStopRequested && ctx.gameRuntime) {
            // Restore default device before teardown so nothing during shutdown sees the game device as default.
            if (ctx.sceneViewPipeline) {
                void* dev = ctx.sceneViewPipeline->GetDevice();
                eyelib::render::rhi::SetDefaultDevice(static_cast<eyelib::render::rhi::IDevice*>(dev));
            }
            try {
                ctx.gameRuntime.reset();
            } catch (...) {
                // Ensure we restore the default device even if shutdown throws.
            }
            ctx.world.SetFrameSnapshot(nullptr);  // clear dangling pointer so next Play uses live world for GetObservation()
            ctx.gameRuntimeStopRequested = false;
            if (ctx.sceneViewPipeline) {
                void* dev = ctx.sceneViewPipeline->GetDevice();
                eyelib::render::rhi::SetDefaultDevice(static_cast<eyelib::render::rhi::IDevice*>(dev));
            }
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
        openeye::editor::panels::ShowSceneViewPanel(&ctx);
        openeye::editor::panels::ShowGameViewPanel(&ctx);

        if (shaderWatcher && ctx.sceneViewPipeline) {
            int copied = shaderWatcher->Tick();
            if (copied > 0)
                ctx.sceneViewPipeline->ReloadShaders();
        }

        ImGui::Render();
        // Restore default framebuffer and full-window viewport so ImGui draws to the window.
        // The scene view pipeline leaves its FBO and a small viewport bound.
        if (ctx.sceneViewPipeline)
            ctx.sceneViewPipeline->RestoreDefaultFramebufferAndViewport(
                static_cast<uint32_t>(window.Width()), static_cast<uint32_t>(window.Height()));
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        window.SwapBuffers();
    }

    return 0;
}

} // namespace openeye::editor
