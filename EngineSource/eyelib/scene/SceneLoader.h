#pragma once

#include "SceneManager.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace eyelib {
namespace scene {

class SceneLoader {
public:
    using LoadCallback = std::function<void(SceneHandle)>;
    using UnloadCallback = std::function<void()>;
    using DependencyResolver = std::function<std::vector<std::string>(const std::string&)>;

    static void LoadSceneAsync(SceneManager& manager, const std::string& path, LoadCallback callback);
    static void UnloadSceneAsync(SceneManager& manager, SceneHandle handle, UnloadCallback callback);

    static float GetProgress();
    static void SetDependencyResolver(DependencyResolver resolver);

private:
    static std::atomic<float> s_progress;
    static DependencyResolver s_resolver;
};

} // namespace scene
} // namespace eyelib
