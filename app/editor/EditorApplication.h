#pragma once

#include "panels/IEditorPanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/AssetPanel.h"
#include "utils/SceneManager.h"
#include "ecs/api/Scene.h"
#include <vector>
#include <memory>

namespace OpenMeta {
class GameApplication;
}

namespace OpenMeta {
namespace Editor {

/**
 * @brief Editor application - provides panels only (no window, no rendering)
 * Editor panels connect to GameApplication's scene
 */
class EditorApplication {
public:
    EditorApplication(OpenMeta::GameApplication* gameApp);
    ~EditorApplication();
    
    bool Initialize();
    void Update(float deltaTime);
    void RenderPanels();
    void Shutdown();

private:
    OpenMeta::GameApplication* m_gameApp = nullptr;
    OpenEye::ECS::Scene* m_scene = nullptr;
    
    std::vector<std::unique_ptr<IEditorPanel>> m_panels;
    
    bool m_isInitialized = false;
};

} // namespace Editor
} // namespace OpenMeta

