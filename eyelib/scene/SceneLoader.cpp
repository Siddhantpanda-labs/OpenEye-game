#include "SceneLoader.h"

#include <thread>

namespace eyelib {
namespace scene {

std::atomic<float> SceneLoader::s_progress{0.0f};
SceneLoader::DependencyResolver SceneLoader::s_resolver{};

void SceneLoader::LoadSceneAsync(SceneManager& manager, const std::string& path, LoadCallback callback) {
    s_progress.store(0.0f);
    std::thread([&manager, path, callback]() {
        if (s_resolver) {
            auto deps = s_resolver(path);
            const size_t total = deps.size() + 1;
            size_t index = 0;
            for (const auto& dep : deps) {
                manager.LoadScene(dep);
                s_progress.store(static_cast<float>(++index) / static_cast<float>(total));
            }
        }
        const SceneHandle handle = manager.LoadScene(path);
        s_progress.store(1.0f);
        if (callback) callback(handle);
    }).detach();
}

void SceneLoader::UnloadSceneAsync(SceneManager& manager, SceneHandle handle, UnloadCallback callback) {
    s_progress.store(0.0f);
    std::thread([&manager, handle, callback]() {
        manager.UnloadScene(handle);
        s_progress.store(1.0f);
        if (callback) callback();
    }).detach();
}

float SceneLoader::GetProgress() {
    return s_progress.load();
}

void SceneLoader::SetDependencyResolver(DependencyResolver resolver) {
    s_resolver = std::move(resolver);
}

} // namespace scene
} // namespace eyelib
