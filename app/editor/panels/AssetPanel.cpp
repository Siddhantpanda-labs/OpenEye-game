#include "AssetPanel.h"
#include "ecs/api/All.h"
#include "core/api/All.h"
#include "imgui.h"
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace OpenMeta {
namespace Editor {

AssetPanel::AssetPanel(OpenEye::ECS::Scene* scene) 
    : m_scene(scene) {
    ScanAssets();
}

void AssetPanel::Render() {
    if (!m_isOpen || !m_scene) return;
    
    ImGui::Begin("Assets", &m_isOpen);
    
    // Directory browser
    if (ImGui::Button("Refresh")) {
        ScanAssets();
    }
    
    ImGui::Separator();
    
    // Tabs for different asset types
    if (ImGui::BeginTabBar("AssetTypes")) {
        if (ImGui::BeginTabItem("Models")) {
            RenderModelBrowser();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Textures")) {
            RenderTextureBrowser();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Materials")) {
            RenderMaterialBrowser();
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

void AssetPanel::ScanAssets() {
    m_modelFiles.clear();
    m_textureFiles.clear();
    
    std::filesystem::path modelsPath = "assets/models";
    if (std::filesystem::exists(modelsPath)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(modelsPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") {
                    m_modelFiles.push_back(entry.path().string());
                }
            }
        }
    }
    
    std::filesystem::path texturesPath = "assets/textures";
    if (std::filesystem::exists(texturesPath)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(texturesPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".hdr" || ext == ".exr") {
                    m_textureFiles.push_back(entry.path().string());
                }
            }
        }
    }
}

void AssetPanel::RenderModelBrowser() {
    ImGui::Text("Models (%zu)", m_modelFiles.size());
    ImGui::Separator();
    
    ImGui::BeginChild("ModelList", ImVec2(0, 0), false);
    
    for (const auto& file : m_modelFiles) {
        std::filesystem::path path(file);
        std::string filename = path.filename().string();
        
        bool isSelected = (m_selectedAsset == file);
        if (ImGui::Selectable(filename.c_str(), isSelected)) {
            m_selectedAsset = file;
        }
        
        // Double-click to add to scene
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            AddModelToScene(file);
        }
        
        // Drag-drop to add to scene
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("MODEL_PATH", file.c_str(), file.length() + 1);
            ImGui::Text("Add %s", filename.c_str());
            ImGui::EndDragDropSource();
        }
    }
    
    ImGui::EndChild();
    
    // Right-click context menu
    if (ImGui::BeginPopupContextWindow("ModelContextMenu")) {
        if (!m_selectedAsset.empty() && ImGui::MenuItem("Add to Scene")) {
            AddModelToScene(m_selectedAsset);
        }
        ImGui::EndPopup();
    }
}

void AssetPanel::RenderTextureBrowser() {
    ImGui::Text("Textures (%zu)", m_textureFiles.size());
    ImGui::Separator();
    
    ImGui::BeginChild("TextureList", ImVec2(0, 0), false);
    
    for (const auto& file : m_textureFiles) {
        std::filesystem::path path(file);
        std::string filename = path.filename().string();
        
        bool isSelected = (m_selectedAsset == file);
        if (ImGui::Selectable(filename.c_str(), isSelected)) {
            m_selectedAsset = file;
        }
    }
    
    ImGui::EndChild();
}

void AssetPanel::RenderMaterialBrowser() {
    ImGui::Text("Materials");
    ImGui::Text("(Material browser coming soon)");
}

void AssetPanel::AddModelToScene(const std::string& path) {
    if (!m_scene) return;
    
    using namespace OpenEye::Core;
    using namespace OpenEye::ECS;
    
    // Use Scene's public API to load model
    OpenEye::ECS::Entity entity = m_scene->LoadModel(path);
    if (entity == OpenEye::ECS::Entity{}) {
        OE_WARN("Failed to load model from: " + path);
        return;
    }
    
    std::filesystem::path filePath(path);
    std::string baseName = filePath.stem().string();
    OE_INFO("Model added to scene: " + baseName);
}

} // namespace Editor
} // namespace OpenMeta

