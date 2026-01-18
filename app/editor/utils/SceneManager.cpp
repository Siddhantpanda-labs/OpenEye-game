#include "SceneManager.h"
#include "core/api/All.h"

namespace OpenMeta {
namespace Editor {

SceneManager& SceneManager::GetInstance() {
    static SceneManager instance;
    return instance;
}

bool SceneManager::LoadScene(OpenEye::ECS::Scene& scene, const std::string& filepath) {
    using namespace OpenEye::Core;
    
    OE_INFO_F("Loading scene from: %s", filepath.c_str());
    
    if (OpenEye::ECS::SceneSerializer::Deserialize(scene, filepath)) {
        m_currentScenePath = filepath;
        m_hasUnsavedChanges = false;
        OE_INFO("Scene loaded successfully");
        return true;
    }
    
    OE_ERROR_F("Failed to load scene from: %s", filepath.c_str());
    return false;
}

bool SceneManager::SaveScene(OpenEye::ECS::Scene& scene, const std::string& filepath) {
    using namespace OpenEye::Core;
    
    OE_INFO_F("Saving scene to: %s", filepath.c_str());
    
    if (OpenEye::ECS::SceneSerializer::Serialize(scene, filepath)) {
        m_currentScenePath = filepath;
        m_hasUnsavedChanges = false;
        OE_INFO("Scene saved successfully");
        return true;
    }
    
    OE_ERROR_F("Failed to save scene to: %s", filepath.c_str());
    return false;
}

} // namespace Editor
} // namespace OpenMeta

