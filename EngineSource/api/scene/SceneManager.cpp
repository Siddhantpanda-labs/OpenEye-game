#include "SceneManager.h"

namespace eyelib {
namespace api {

class SceneManager::Impl {
public:
    eyelib::scene::SceneManager manager{};
};

SceneManager::SceneManager() : m_impl(std::make_unique<Impl>()) {}
SceneManager::~SceneManager() = default;

SceneHandle SceneManager::CreateScene(const std::string& name) {
    return m_impl->manager.CreateScene(name);
}

SceneHandle SceneManager::LoadScene(const std::string& path) {
    return m_impl->manager.LoadScene(path);
}

void SceneManager::UnloadScene(SceneHandle handle) {
    m_impl->manager.UnloadScene(handle);
}

SceneHandle SceneManager::GetActiveScene() const {
    return m_impl->manager.GetActiveScene();
}

void SceneManager::SetActiveScene(SceneHandle handle) {
    m_impl->manager.SetActiveScene(handle);
}

std::shared_ptr<Scene> SceneManager::GetScene(SceneHandle handle) {
    auto* scene = m_impl->manager.GetScene(handle);
    return Scene::WrapInternal(scene);
}

const std::string& SceneManager::GetSceneName(SceneHandle handle) const {
    return m_impl->manager.GetSceneName(handle);
}

std::vector<SceneHandle> SceneManager::GetSceneHandles() const {
    return m_impl->manager.GetSceneHandles();
}

void SceneManager::LoadSceneAsync(const std::string& path, eyelib::scene::SceneLoader::LoadCallback callback) {
    eyelib::scene::SceneLoader::LoadSceneAsync(m_impl->manager, path, std::move(callback));
}

void SceneManager::UnloadSceneAsync(SceneHandle handle, eyelib::scene::SceneLoader::UnloadCallback callback) {
    eyelib::scene::SceneLoader::UnloadSceneAsync(m_impl->manager, handle, std::move(callback));
}

float SceneManager::GetLoadProgress() const {
    return eyelib::scene::SceneLoader::GetProgress();
}

void SceneManager::SetDependencyResolver(eyelib::scene::SceneLoader::DependencyResolver resolver) {
    eyelib::scene::SceneLoader::SetDependencyResolver(std::move(resolver));
}

} // namespace api
} // namespace eyelib
