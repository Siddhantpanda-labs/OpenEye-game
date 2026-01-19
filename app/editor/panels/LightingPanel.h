#pragma once

#include "IEditorPanel.h"
#include "ecs/api/Scene.h"
#include "rendering/api/RenderPipeline.h"

namespace OpenMeta {
namespace Editor {

/**
 * @brief Lighting panel - control directional, spot, and point lights
 */
class LightingPanel : public IEditorPanel {
public:
    LightingPanel(OpenEye::ECS::Scene* scene, OpenEye::Rendering::RenderPipeline* renderPipeline);
    
    void Render() override;
    const char* GetName() const override { return "Lighting"; }
    
private:
    void RenderDirectionalLight();
    void RenderSpotLight();
    void RenderPointLight();
    void RenderSkySettings();
    
    OpenEye::ECS::Scene* m_scene = nullptr;
    OpenEye::Rendering::RenderPipeline* m_renderPipeline = nullptr;
    
    // UI state
    int m_selectedLightIndex = -1;
    bool m_showAddLightMenu = false;
};

} // namespace Editor
} // namespace OpenMeta
