#include "Overlay.h"

#include "api/core/All.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

namespace openeye::player {

bool RuntimeOverlay::Initialize(eyelib::api::Window& window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(window.GetNativeHandle()), true);
    ImGui_ImplOpenGL3_Init("#version 450");
    m_ready = true;
    return true;
}

void RuntimeOverlay::Render(const OverlaySnapshot& snapshot) {
    if (!m_ready) {
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoNav;
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("Runtime Overlay", nullptr, flags)) {
        ImGui::Text("Scene: %s", snapshot.scenePath.c_str());
        ImGui::Text("FPS: %.1f", snapshot.fps);
        ImGui::Text("Frame: %.3f ms", snapshot.frameTimeMs);
        ImGui::Text("Entities: %zu", snapshot.entityCount);
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void RuntimeOverlay::Shutdown() {
    if (!m_ready) {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_ready = false;
}

} // namespace openeye::player
