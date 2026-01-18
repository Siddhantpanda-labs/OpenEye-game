#pragma once

#include "IEditorPanel.h"
#include "ecs/api/Scene.h"
#include <string>
#include <vector>

namespace OpenMeta {
namespace Editor {

/**
 * @brief Asset panel - browse and add assets to scene
 */
class AssetPanel : public IEditorPanel {
public:
    AssetPanel(OpenEye::ECS::Scene* scene);
    
    void Render() override;
    const char* GetName() const override { return "Assets"; }
    
    void AddModelToScene(const std::string& path); // Public for drag-drop from viewport

private:
    void ScanAssets();
    void RenderModelBrowser();
    void RenderTextureBrowser();
    void RenderMaterialBrowser();
    
    OpenEye::ECS::Scene* m_scene = nullptr;
    std::string m_currentDirectory = "assets/models";
    std::vector<std::string> m_modelFiles;
    std::vector<std::string> m_textureFiles;
    std::string m_selectedAsset;
};

} // namespace Editor
} // namespace OpenMeta

