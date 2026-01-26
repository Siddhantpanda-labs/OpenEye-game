#include "LuaVM.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include "eyelib/core/Log.h"
#include <cstring>

namespace eyelib {
namespace scripting {

LuaVM::LuaVM() {
    Initialize();
}

LuaVM::~LuaVM() {
    Shutdown();
}

std::shared_ptr<LuaVM> LuaVM::Create() {
    return std::shared_ptr<LuaVM>(new LuaVM());
}

void LuaVM::Initialize() {
    m_luaState = luaL_newstate();
    if (!m_luaState) {
        core::Log::Error("Failed to create Lua state");
        return;
    }

    // Open standard libraries (will be restricted by sandbox)
    luaL_openlibs(m_luaState);

    // Configure sandbox
    ConfigureSandbox(m_sandboxMode);
}

void LuaVM::Shutdown() {
    if (m_luaState) {
        lua_close(m_luaState);
        m_luaState = nullptr;
    }
}

void LuaVM::SetSandboxMode(SandboxMode mode) {
    m_sandboxMode = mode;
    if (m_luaState) {
        ConfigureSandbox(mode);
    }
}

void LuaVM::ConfigureSandbox(SandboxMode mode) {
    if (!m_luaState) return;

    switch (mode) {
        case SandboxMode::Restricted: {
            // Remove dangerous functions
            RemoveFunction("os.execute");
            RemoveFunction("os.exit");
            RemoveFunction("os.remove");
            RemoveFunction("os.rename");
            RemoveFunction("os.tmpname");
            RemoveFunction("io.popen");
            RemoveFunction("io.open");
            RemoveFunction("io.input");
            RemoveFunction("io.output");
            RemoveFunction("io.close");
            RemoveFunction("io.read");
            RemoveFunction("io.write");
            RemoveFunction("io.flush");
            RemoveFunction("io.lines");
            RemoveFunction("io.type");
            RemoveFunction("io.tmpfile");
            RemoveFunction("package.loadlib");
            RemoveFunction("package.loaders");
            RemoveFunction("package.searchers");
            RemoveFunction("require");
            RemoveFunction("dofile");
            RemoveFunction("loadfile");
            RemoveFunction("load");
            RemoveFunction("loadstring");
            
            // Keep safe functions: math, string, table, basic lua functions
            // These are already loaded by luaL_openlibs
            break;
        }
        case SandboxMode::Standard: {
            // Allow standard Lua libraries but restrict os/io
            RemoveFunction("os.execute");
            RemoveFunction("os.exit");
            RemoveFunction("io.popen");
            break;
        }
        case SandboxMode::Permissive: {
            // Allow almost everything (dev only)
            break;
        }
    }
}

void LuaVM::RemoveFunction(const std::string& name) {
    if (!m_luaState) return;

    // Parse function name (e.g., "os.execute" -> table "os", key "execute")
    size_t dotPos = name.find('.');
    if (dotPos != std::string::npos) {
        std::string tableName = name.substr(0, dotPos);
        std::string funcName = name.substr(dotPos + 1);

        lua_getglobal(m_luaState, tableName.c_str());
        if (lua_istable(m_luaState, -1)) {
            lua_pushnil(m_luaState);
            lua_setfield(m_luaState, -2, funcName.c_str());
        }
        lua_pop(m_luaState, 1);
    } else {
        // Global function
        lua_pushnil(m_luaState);
        lua_setglobal(m_luaState, name.c_str());
    }
}

void LuaVM::AllowFunction(const std::string& funcName) {
    // For now, we only restrict - allowing would require storing original functions
    // This is a simplified implementation
    core::Log::WarnF("LuaVM::AllowFunction not fully implemented: %s", funcName.c_str());
}

void LuaVM::DisallowFunction(const std::string& funcName) {
    RemoveFunction(funcName);
}

void LuaVM::SetMemoryLimit(size_t bytes) {
    m_memoryLimit = bytes;
    // Lua memory limit would be set via lua_gc, but for now we track it manually
}

size_t LuaVM::GetMemoryUsage() const {
    if (!m_luaState) return 0;
    // Get Lua memory usage via lua_gc
    return lua_gc(m_luaState, LUA_GCCOUNT, 0) * 1024 + lua_gc(m_luaState, LUA_GCCOUNTB, 0);
}

bool LuaVM::ExecuteString(const std::string& code, std::string& error) {
    if (!m_luaState) {
        error = "Lua state is not valid";
        return false;
    }

    int result = luaL_loadstring(m_luaState, code.c_str());
    if (result != LUA_OK) {
        error = lua_tostring(m_luaState, -1);
        lua_pop(m_luaState, 1);
        if (m_errorCallback) {
            m_errorCallback(error);
        }
        return false;
    }

    result = lua_pcall(m_luaState, 0, LUA_MULTRET, 0);
    if (result != LUA_OK) {
        error = lua_tostring(m_luaState, -1);
        lua_pop(m_luaState, 1);
        if (m_errorCallback) {
            m_errorCallback(error);
        }
        return false;
    }

    return true;
}

bool LuaVM::ExecuteFile(const std::string& path, std::string& error) {
    if (!m_luaState) {
        error = "Lua state is not valid";
        return false;
    }

    int result = luaL_loadfile(m_luaState, path.c_str());
    if (result != LUA_OK) {
        error = lua_tostring(m_luaState, -1);
        lua_pop(m_luaState, 1);
        if (m_errorCallback) {
            m_errorCallback(error);
        }
        return false;
    }

    result = lua_pcall(m_luaState, 0, LUA_MULTRET, 0);
    if (result != LUA_OK) {
        error = lua_tostring(m_luaState, -1);
        lua_pop(m_luaState, 1);
        if (m_errorCallback) {
            m_errorCallback(error);
        }
        return false;
    }

    return true;
}

} // namespace scripting
} // namespace eyelib
