#include "EditorApplication.h"
#include "../openmeta/src/core/GameApplication.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/AssetPanel.h"
#include "utils/SceneManager.h"
#include "core/api/All.h"
#include "imgui.h"

namespace OpenMeta {
namespace Editor {

EditorApplication::EditorApplication(OpenMeta::GameApplication* gameApp) 
    : m_gameApp(gameApp) {
    if (gameApp) {
        m_scene = gameApp->GetScene();
    }
}

EditorApplication::~EditorApplication() {
    Shutdown();
}

bool EditorApplication::Initialize() {
    using namespace OpenEye::Core;
    
    if (!m_gameApp || !m_scene) {
        OE_ERROR("EditorApplication: GameApplication or Scene is null");
        return false;
    }
    
    OE_INFO("Initializing Editor Panels...");
    
    // Create panels (they connect to GameApplication's scene)
    m_panels.push_back(std::make_unique<HierarchyPanel>(m_scene));
    m_panels.push_back(std::make_unique<InspectorPanel>(m_scene));
    m_panels.push_back(std::make_unique<AssetPanel>(m_scene));
    
    m_isInitialized = true;
    OE_INFO("Editor panels initialized successfully");
    return true;
}

void EditorApplication::Update(float deltaTime) {
    // Editor doesn't need to update anything - game handles it
    // This is just here for consistency
}

void EditorApplication::RenderPanels() {
    if (!m_isInitialized) return;
    
    // Menu bar for editor
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Scene")) {
                auto& sceneManager = SceneManager::GetInstance();
                if (!sceneManager.GetCurrentScenePath().empty()) {
                    sceneManager.SaveScene(*m_scene, sceneManager.GetCurrentScenePath());
                } else {
                    // Default scene path
                    sceneManager.SaveScene(*m_scene, "resources/scenes/default.json");
                }
            }
            if (ImGui::MenuItem("Save Scene As")) {
                // TODO: Save scene dialog
                auto& sceneManager = SceneManager::GetInstance();
                sceneManager.SaveScene(*m_scene, "resources/scenes/default.json");
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            for (auto& panel : m_panels) {
                bool isOpen = panel->IsOpen();
                if (ImGui::MenuItem(panel->GetName(), nullptr, &isOpen)) {
                    panel->SetOpen(isOpen);
                }
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
    
    // Render all panels
    for (auto& panel : m_panels) {
        if (panel->IsOpen()) {
            panel->Render();
        }
    }
}

void EditorApplication::Shutdown() {
    using namespace OpenEye::Core;
    
    OE_INFO("Shutting down editor panels...");
    
    m_panels.clear();
    m_isInitialized = false;
}

} // namespace Editor
} // namespace OpenMeta

