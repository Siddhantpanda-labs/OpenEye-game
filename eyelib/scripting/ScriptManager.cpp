#include "ScriptManager.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <fstream>
#include <sstream>
#include <ctime>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace eyelib {
namespace scripting {

ScriptManager& ScriptManager::Get() {
    static ScriptManager instance;
    return instance;
}

ScriptHandle ScriptManager::LoadScript(const std::string& path) {
    // Check if already loaded
    auto it = m_pathToHandle.find(path);
    if (it != m_pathToHandle.end()) {
        return it->second;
    }

    ScriptHandle handle = m_nextHandle++;
    ScriptInfo info;
    info.handle = handle;
    info.path = path;
    info.vm = LuaVM::Create();
    info.lastModified = GetFileModificationTime(path);

    if (LoadScriptContent(info)) {
        info.isValid = true;
        core::Log::InfoF("Loaded script: %s (handle: %zu)", path.c_str(), handle);
    } else {
        info.isValid = false;
        core::Log::ErrorF("Failed to load script: %s - %s", path.c_str(), info.lastError.c_str());
    }

    m_scripts[handle] = info;
    m_pathToHandle[path] = handle;

    return handle;
}

void ScriptManager::UnloadScript(ScriptHandle handle) {
    auto it = m_scripts.find(handle);
    if (it != m_scripts.end()) {
        m_pathToHandle.erase(it->second.path);
        m_scripts.erase(it);
    }
}

bool ScriptManager::ReloadScript(ScriptHandle handle) {
    auto* info = GetScriptInfo(handle);
    if (!info) {
        return false;
    }

    std::time_t newModTime = GetFileModificationTime(info->path);
    if (newModTime == info->lastModified) {
        return true; // No changes
    }

    info->lastModified = newModTime;
    info->isValid = false;
    info->lastError.clear();

    // Create new VM for reload
    info->vm = LuaVM::Create();

    if (LoadScriptContent(*info)) {
        info->isValid = true;
        core::Log::InfoF("Reloaded script: %s", info->path.c_str());
        
        if (info->reloadCallback) {
            info->reloadCallback();
        }
        return true;
    } else {
        core::Log::ErrorF("Failed to reload script: %s - %s", info->path.c_str(), info->lastError.c_str());
        return false;
    }
}

void ScriptManager::CheckForChanges() {
    if (!m_hotReloadEnabled) return;

    for (auto& [handle, info] : m_scripts) {
        std::time_t newModTime = GetFileModificationTime(info.path);
        if (newModTime != info.lastModified && newModTime != 0) {
            ReloadScript(handle);
        }
    }
}

void ScriptManager::SetReloadCallback(ScriptHandle handle, std::function<void()> callback) {
    auto* info = GetScriptInfo(handle);
    if (info) {
        info->reloadCallback = callback;
    }
}

std::string ScriptManager::GetScriptPath(ScriptHandle handle) const {
    const auto* info = GetScriptInfo(handle);
    return info ? info->path : "";
}

std::string ScriptManager::GetScriptName(ScriptHandle handle) const {
    const auto* info = GetScriptInfo(handle);
    return info ? info->scriptName : "";
}

std::string ScriptManager::GetScriptError(ScriptHandle handle) const {
    const auto* info = GetScriptInfo(handle);
    return info ? info->lastError : "";
}

bool ScriptManager::IsScriptValid(ScriptHandle handle) const {
    const auto* info = GetScriptInfo(handle);
    return info ? info->isValid : false;
}

std::shared_ptr<LuaVM> ScriptManager::GetScriptVM(ScriptHandle handle) const {
    const auto* info = GetScriptInfo(handle);
    return info ? info->vm : nullptr;
}

std::vector<ScriptHandle> ScriptManager::GetDependencies(ScriptHandle handle) const {
    const auto* info = GetScriptInfo(handle);
    return info ? info->dependencies : std::vector<ScriptHandle>();
}

ScriptManager::ScriptInfo* ScriptManager::GetScriptInfo(ScriptHandle handle) {
    auto it = m_scripts.find(handle);
    return (it != m_scripts.end()) ? &it->second : nullptr;
}

const ScriptManager::ScriptInfo* ScriptManager::GetScriptInfo(ScriptHandle handle) const {
    auto it = m_scripts.find(handle);
    return (it != m_scripts.end()) ? &it->second : nullptr;
}

std::time_t ScriptManager::GetFileModificationTime(const std::string& path) const {
    // Use platform filesystem to get modification time
    int64_t fileTime = platform::filesystem::GetFileTime(path);
    if (fileTime <= 0) {
        return 0;
    }
    return static_cast<std::time_t>(fileTime);
}

bool ScriptManager::LoadScriptContent(ScriptInfo& info) {
    // Read file content
    std::ifstream file(info.path);
    if (!file.is_open()) {
        info.lastError = "Failed to open file";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    info.content = buffer.str();
    file.close();

    // Execute script in VM - script should return a table
    std::string error;
    lua_State* L = info.vm->GetState();
    
    // Load and execute script
    int result = luaL_loadstring(L, info.content.c_str());
    if (result != LUA_OK) {
        info.lastError = lua_tostring(L, -1);
        lua_pop(L, 1);
        return false;
    }
    
    result = lua_pcall(L, 0, 1, 0); // Expect 1 return value (the script table)
    if (result != LUA_OK) {
        info.lastError = lua_tostring(L, -1);
        lua_pop(L, 1);
        return false;
    }
    
    // Check if return value is a table
    if (!lua_istable(L, -1)) {
        info.lastError = "Script must return a table";
        lua_pop(L, 1);
        return false;
    }
    
    // Store the table globally with script name (extracted from path)
    // Extract script name from path (e.g., "assets/scripts/TestScript.lua" -> "TestScript")
    std::string scriptName = info.path;
    size_t lastSlash = scriptName.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        scriptName = scriptName.substr(lastSlash + 1);
    }
    size_t lastDot = scriptName.find_last_of(".");
    if (lastDot != std::string::npos) {
        scriptName = scriptName.substr(0, lastDot);
    }
    
    info.scriptName = scriptName;
    
    // Store table as global with script name
    lua_setglobal(L, scriptName.c_str());
    
    return true;
}

} // namespace scripting
} // namespace eyelib
