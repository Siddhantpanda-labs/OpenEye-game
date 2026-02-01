#pragma once

#include "LuaVM.h"
#include "ScriptHandle.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace eyelib {
namespace scripting {

/**
 * @brief Manages script loading, caching, and hot-reloading.
 */
class ScriptManager {
public:
    static ScriptManager& Get();

    // Script loading
    ScriptHandle LoadScript(const std::string& path);
    void UnloadScript(ScriptHandle handle);
    bool ReloadScript(ScriptHandle handle);

    // Hot-reloading
    void EnableHotReload(bool enabled) { m_hotReloadEnabled = enabled; }
    void CheckForChanges(); // Call per frame
    void SetReloadCallback(ScriptHandle handle, std::function<void()> callback);

    // Script info
    std::string GetScriptPath(ScriptHandle handle) const;
    std::string GetScriptName(ScriptHandle handle) const;
    std::string GetScriptError(ScriptHandle handle) const;
    bool IsScriptValid(ScriptHandle handle) const;
    std::shared_ptr<LuaVM> GetScriptVM(ScriptHandle handle) const;

    bool CallUpdate(ScriptHandle handle, float deltaTime);
    bool CallStart(ScriptHandle handle);
    bool CallStop(ScriptHandle handle);

    // Dependency tracking
    std::vector<ScriptHandle> GetDependencies(ScriptHandle handle) const;

private:
    ScriptManager() = default;
    ~ScriptManager() = default;

    ScriptManager(const ScriptManager&) = delete;
    ScriptManager& operator=(const ScriptManager&) = delete;

    struct ScriptInfo {
        ScriptHandle handle;
        std::string path;
        std::string scriptName;  // Extracted from path (e.g., "TestScript")
        std::string content;
        std::string lastError;
        std::shared_ptr<LuaVM> vm;
        bool isValid = false;
        std::time_t lastModified = 0;
        std::function<void()> reloadCallback;
        std::vector<ScriptHandle> dependencies;
    };

    ScriptHandle m_nextHandle = 1;
    std::unordered_map<ScriptHandle, ScriptInfo> m_scripts;
    std::unordered_map<std::string, ScriptHandle> m_pathToHandle;
    bool m_hotReloadEnabled = false;

    ScriptInfo* GetScriptInfo(ScriptHandle handle);
    const ScriptInfo* GetScriptInfo(ScriptHandle handle) const;
    std::time_t GetFileModificationTime(const std::string& path) const;
    bool LoadScriptContent(ScriptInfo& info);
};

} // namespace scripting
} // namespace eyelib
