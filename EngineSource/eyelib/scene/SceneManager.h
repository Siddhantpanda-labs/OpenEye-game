#pragma once

#include "Scene.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace scene {

using SceneHandle = uint32_t;
static constexpr SceneHandle NullScene = 0;

class SceneManager {
public:
    SceneHandle CreateScene(const std::string& name);
    SceneHandle LoadScene(const std::string& path);
    void UnloadScene(SceneHandle handle);

    SceneHandle GetActiveScene() const;
    void SetActiveScene(SceneHandle handle);

    Scene* GetScene(SceneHandle handle);
    const Scene* GetScene(SceneHandle handle) const;

    const std::string& GetSceneName(SceneHandle handle) const;
    const std::vector<SceneHandle>& GetSceneHandles() const;

private:
    struct Entry {
        std::string name;
        std::unique_ptr<Scene> scene;
    };

    SceneHandle m_nextHandle = 1;
    SceneHandle m_active = NullScene;
    std::unordered_map<SceneHandle, Entry> m_scenes;
    std::vector<SceneHandle> m_handles;
};

} // namespace scene
} // namespace eyelib
