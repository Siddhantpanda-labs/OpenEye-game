#include "SceneManager.h"

#include "eyelib/serialize/BinarySerializer.h"

#include <algorithm>

namespace eyelib {
namespace scene {

SceneHandle SceneManager::CreateScene(const std::string& name) {
    SceneHandle handle = m_nextHandle++;
    Entry entry;
    entry.name = name;
    entry.scene = std::make_unique<Scene>();
    m_scenes.emplace(handle, std::move(entry));
    m_handles.push_back(handle);
    if (m_active == NullScene) m_active = handle;
    return handle;
}

SceneHandle SceneManager::LoadScene(const std::string& path) {
    SceneHandle handle = m_nextHandle++;
    Entry entry;
    entry.name = path;
    entry.scene = std::make_unique<Scene>();
    serialize::BinarySerializer::LoadFromFile(*entry.scene, path);
    m_scenes.emplace(handle, std::move(entry));
    m_handles.push_back(handle);
    if (m_active == NullScene) m_active = handle;
    return handle;
}

void SceneManager::UnloadScene(SceneHandle handle) {
    if (handle == NullScene) return;
    m_scenes.erase(handle);
    m_handles.erase(std::remove(m_handles.begin(), m_handles.end(), handle), m_handles.end());
    if (m_active == handle) {
        m_active = m_handles.empty() ? NullScene : m_handles.front();
    }
}

SceneHandle SceneManager::GetActiveScene() const {
    return m_active;
}

void SceneManager::SetActiveScene(SceneHandle handle) {
    if (m_scenes.find(handle) == m_scenes.end()) return;
    m_active = handle;
}

Scene* SceneManager::GetScene(SceneHandle handle) {
    auto it = m_scenes.find(handle);
    return it != m_scenes.end() ? it->second.scene.get() : nullptr;
}

const Scene* SceneManager::GetScene(SceneHandle handle) const {
    auto it = m_scenes.find(handle);
    return it != m_scenes.end() ? it->second.scene.get() : nullptr;
}

const std::string& SceneManager::GetSceneName(SceneHandle handle) const {
    static const std::string kEmpty{};
    auto it = m_scenes.find(handle);
    return it != m_scenes.end() ? it->second.name : kEmpty;
}

const std::vector<SceneHandle>& SceneManager::GetSceneHandles() const {
    return m_handles;
}

} // namespace scene
} // namespace eyelib
