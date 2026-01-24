#pragma once

#include "Scene.h"

#include "eyelib/scene/SceneLoader.h"
#include "eyelib/scene/SceneManager.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace api {

using SceneHandle = uint32_t;

class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    SceneHandle CreateScene(const std::string& name);
    SceneHandle LoadScene(const std::string& path);
    void UnloadScene(SceneHandle handle);

    SceneHandle GetActiveScene() const;
    void SetActiveScene(SceneHandle handle);

    std::shared_ptr<Scene> GetScene(SceneHandle handle);
    const std::string& GetSceneName(SceneHandle handle) const;
    std::vector<SceneHandle> GetSceneHandles() const;

    void LoadSceneAsync(const std::string& path, eyelib::scene::SceneLoader::LoadCallback callback);
    void UnloadSceneAsync(SceneHandle handle, eyelib::scene::SceneLoader::UnloadCallback callback);
    float GetLoadProgress() const;
    void SetDependencyResolver(eyelib::scene::SceneLoader::DependencyResolver resolver);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
