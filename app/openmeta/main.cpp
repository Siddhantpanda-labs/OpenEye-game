// OpenMeta Game Application Entry Point
// Main window and game rendering

#include "src/core/GameApplication.h"
#include <exception>
#include <cstdlib>

#ifdef OPENEYE_EDITOR
#include "editor/EditorApplication.h"
#include "core/api/All.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif
#endif

int main() {
    OpenMeta::GameApplication game;

    try {
        if (!game.Initialize()) {
            return -1;
        }

        // Catch unhandled exceptions/terminates and log them (helps diagnose "aborting after log").
        std::set_terminate([]() {
            try {
                auto ex = std::current_exception();
                if (ex) std::rethrow_exception(ex);
            } catch (const std::exception& e) {
                OE_ERROR_F("std::terminate due to exception: %s", e.what());
            } catch (...) {
                OE_ERROR("std::terminate due to unknown exception");
            }
            std::abort();
        });

#if defined(_MSC_VER) && defined(_DEBUG)
        // Log CRT asserts instead of showing modal dialogs (so we can see the reason in console logs).
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif
    
#ifdef OPENEYE_EDITOR
    // Initialize ImGui for editor panels (only in debug builds)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // Disable viewports to prevent crashes when dragging floating windows
    // Viewports create separate platform windows which can cause OpenGL context issues
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui::StyleColorsDark();
    
    ImGuiStyle& style = ImGui::GetStyle();
    // Viewport-specific styling disabled since viewports are disabled
    // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    //     style.WindowRounding = 0.0f;
    //     style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    // }
    
    ImGui_ImplGlfw_InitForOpenGL(game.GetWindow()->GetNativeHandle(), true);
    ImGui_ImplOpenGL3_Init("#version 450");
    
    // Initialize editor panels
    OpenMeta::Editor::EditorApplication editor(&game);
    if (!editor.Initialize()) {
        OE_ERROR("Failed to initialize editor");
        return -1;
    }
    
    // Main loop with editor
    using namespace OpenEye::Core;
    while (game.GetWindow() && !game.GetWindow()->ShouldClose()) {
        Time::Update();
        game.GetWindow()->PollEvents();
        Input::Update();
        
        // ImGui new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render editor panels first (they will float as separate windows)
        editor.RenderPanels();
        
        // Update and render game (single frame, not full loop)
        // Game scene renders directly to main window (not covered by dockspace)
        float deltaTime = Time::DeltaTime();
        game.Update(deltaTime);
        game.Render();
        
        // Render ImGui (panels already rendered above, now render draw data)
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Viewport handling disabled to prevent crashes when dragging floating windows
        // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        //     ImGui::UpdatePlatformWindows();
        //     ImGui::RenderPlatformWindowsDefault();
        // }
        
        game.GetWindow()->SwapBuffers();
    }
    
    editor.Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#else
    // Release build: game only, no editor
    game.Run();
#endif
    
    game.Shutdown();
    return 0;
    } catch (const std::exception& e) {
        OE_ERROR_F("Fatal exception in main: %s", e.what());
        game.Shutdown();
        return -2;
    } catch (...) {
        OE_ERROR("Fatal unknown exception in main");
        game.Shutdown();
        return -3;
    }
}
