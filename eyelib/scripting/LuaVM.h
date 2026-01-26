#pragma once

#include <memory>
#include <string>
#include <functional>

struct lua_State;

namespace eyelib {
namespace scripting {

enum class SandboxMode {
    Restricted,  // Default: only engine APIs
    Standard,    // Standard Lua libraries
    Permissive   // Almost everything (dev only)
};

/**
 * @brief Manages a Lua virtual machine with sandboxing and error handling.
 */
class LuaVM {
public:
    static std::shared_ptr<LuaVM> Create();
    ~LuaVM();

    LuaVM(const LuaVM&) = delete;
    LuaVM& operator=(const LuaVM&) = delete;

    // State management
    lua_State* GetState() const { return m_luaState; }
    bool IsValid() const { return m_luaState != nullptr; }

    // Sandboxing
    void SetSandboxMode(SandboxMode mode);
    void AllowFunction(const std::string& funcName);
    void DisallowFunction(const std::string& funcName);

    // Memory limits
    void SetMemoryLimit(size_t bytes);
    size_t GetMemoryUsage() const;

    // Error handling
    bool ExecuteString(const std::string& code, std::string& error);
    bool ExecuteFile(const std::string& path, std::string& error);

    // Callbacks
    void SetErrorCallback(std::function<void(const std::string&)> callback) {
        m_errorCallback = callback;
    }

private:
    LuaVM();
    void Initialize();
    void Shutdown();
    void ConfigureSandbox(SandboxMode mode);
    void RemoveFunction(const std::string& name);
    void AddCustomFunction(const std::string& name, int (*func)(lua_State*));

    lua_State* m_luaState = nullptr;
    SandboxMode m_sandboxMode = SandboxMode::Restricted;
    size_t m_memoryLimit = 0; // 0 = unlimited
    std::function<void(const std::string&)> m_errorCallback;
};

} // namespace scripting
} // namespace eyelib
