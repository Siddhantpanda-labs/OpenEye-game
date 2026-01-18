// OpenMeta Game Application Entry Point
// Main window and game rendering

#include "src/core/GameApplication.h"

#ifdef OPENEYE_EDITOR
#include "editor/EditorApplication.h"
#include "core/api/All.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#endif

int main() {
    OpenMeta::GameApplication game;
    
    if (!game.Initialize()) {
        return -1;
    }
    
#ifdef OPENEYE_EDITOR
    // Initialize ImGui for editor panels (only in debug builds)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui::StyleColorsDark();
    
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    
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
        
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        
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
}
