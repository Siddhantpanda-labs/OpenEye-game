#pragma once

#include "ecs/api/All.h"
#include <string>

namespace OpenMeta {
namespace Editor {

/**
 * @brief Manages scene loading and saving
 */
class SceneManager {
public:
    static SceneManager& GetInstance();
    
    bool LoadScene(OpenEye::ECS::Scene& scene, const std::string& filepath);
    bool SaveScene(OpenEye::ECS::Scene& scene, const std::string& filepath);
    
    const std::string& GetCurrentScenePath() const { return m_currentScenePath; }
    void SetCurrentScenePath(const std::string& path) { m_currentScenePath = path; }
    
    bool HasUnsavedChanges() const { return m_hasUnsavedChanges; }
    void SetUnsavedChanges(bool changed) { m_hasUnsavedChanges = changed; }

private:
    SceneManager() = default;
    std::string m_currentScenePath;
    bool m_hasUnsavedChanges = false;
};

} // namespace Editor
} // namespace OpenMeta

