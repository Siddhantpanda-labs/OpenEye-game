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

namespace {

bool CallFunction(LuaVM& vm, const std::string& tableName, const char* functionName, bool withDelta, float deltaTime, std::string& errorOut) {
    lua_State* L = vm.GetState();
    lua_getglobal(L, tableName.c_str());
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }

    lua_getfield(L, -1, functionName);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return false;
    }

    lua_pushvalue(L, -2); // self table
    int argc = 1;
    if (withDelta) {
        lua_pushnumber(L, static_cast<lua_Number>(deltaTime));
        argc = 2;
    }

    if (lua_pcall(L, argc, 0, 0) != LUA_OK) {
        errorOut = lua_tostring(L, -1);
        lua_pop(L, 1);
        lua_pop(L, 1); // pop table
        return false;
    }

    lua_pop(L, 1); // pop table
    return true;
}

} // namespace

bool ScriptManager::CallUpdate(ScriptHandle handle, float deltaTime) {
    auto* info = GetScriptInfo(handle);
    if (!info || !info->isValid || !info->vm) {
        return false;
    }
    std::string err;
    const bool ok = CallFunction(*info->vm, info->scriptName, "Update", true, deltaTime, err);
    if (!ok && !err.empty()) {
        core::Log::WarnF("Script Update failed: %s - %s", info->scriptName.c_str(), err.c_str());
    }
    return ok;
}

bool ScriptManager::CallStart(ScriptHandle handle) {
    auto* info = GetScriptInfo(handle);
    if (!info || !info->isValid || !info->vm) {
        return false;
    }
    std::string err;
    const bool ok = CallFunction(*info->vm, info->scriptName, "Start", false, 0.0f, err);
    if (!ok && !err.empty()) {
        core::Log::WarnF("Script Start failed: %s - %s", info->scriptName.c_str(), err.c_str());
    }
    return ok;
}

bool ScriptManager::CallStop(ScriptHandle handle) {
    auto* info = GetScriptInfo(handle);
    if (!info || !info->isValid || !info->vm) {
        return false;
    }
    std::string err;
    const bool ok = CallFunction(*info->vm, info->scriptName, "Stop", false, 0.0f, err);
    if (!ok && !err.empty()) {
        core::Log::WarnF("Script Stop failed: %s - %s", info->scriptName.c_str(), err.c_str());
    }
    return ok;
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
