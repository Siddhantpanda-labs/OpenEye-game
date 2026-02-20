#include "EditorApp.h"

#include "EditorContext.h"
#include "MenuBar.h"
#include "panels/StatusPanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/SceneViewPanel.h"

#include "api/assets/All.h"
#include "api/core/All.h"
#include "api/platform/All.h"
#include "api/render/All.h"
#include "eyelib/render/RenderBackend.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
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
    eyelib::api::AssetRegistry assetRegistry;

    openeye::editor::EditorContext ctx;
    ctx.sceneViewPipeline = &sceneViewPipeline;
    ctx.assetRegistry = &assetRegistry;
    ctx.assetRoots = { "assets" };

    while (!window.ShouldClose()) {
        window.PollEvents();
        window.MakeContextCurrent();
        window.RefreshFramebufferSize();
        window.Clear(0.2f, 0.2f, 0.25f, 1.0f);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        openeye::editor::ShowMenuBar(ctx);

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        openeye::editor::panels::ShowStatusPanel(&ctx);
        openeye::editor::panels::ShowHierarchyPanel(&ctx);
        openeye::editor::panels::ShowInspectorPanel(&ctx);
        openeye::editor::panels::ShowSceneViewPanel(&ctx);

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
