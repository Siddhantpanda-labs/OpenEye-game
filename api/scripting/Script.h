#pragma once

#include <memory>
#include <string>

#include "eyelib/scripting/ScriptHandle.h"

struct lua_State;

namespace eyelib {
namespace api {

/**
 * @brief Script handle - represents a loaded Lua script.
 * Uses PIMPL to hide implementation details.
 */
class Script {
public:
    Script();
    ~Script();

    Script(const Script&) = delete;
    Script& operator=(const Script&) = delete;
    Script(Script&& other) noexcept;
    Script& operator=(Script&& other) noexcept;

    /**
     * @brief Load a script from file path.
     * @return ScriptHandle (0 if failed)
     */
    static ::eyelib::scripting::ScriptHandle Load(const std::string& path);
    
    /**
     * @brief Unload a script.
     */
    static void Unload(::eyelib::scripting::ScriptHandle handle);
    
    /**
     * @brief Reload a script (hot-reload).
     * @return true if successful
     */
    static bool Reload(::eyelib::scripting::ScriptHandle handle);
    
    /**
     * @brief Get script path.
     */
    static std::string GetPath(::eyelib::scripting::ScriptHandle handle);
    
    /**
     * @brief Get script name (extracted from path).
     */
    static std::string GetName(::eyelib::scripting::ScriptHandle handle);
    
    /**
     * @brief Check if script is valid.
     */
    static bool IsValid(::eyelib::scripting::ScriptHandle handle);
    
    /**
     * @brief Get last error message for script.
     */
    static std::string GetError(::eyelib::scripting::ScriptHandle handle);

    static bool CallStart(::eyelib::scripting::ScriptHandle handle);
    static bool CallUpdate(::eyelib::scripting::ScriptHandle handle, float deltaTime);
    static bool CallStop(::eyelib::scripting::ScriptHandle handle);
    static lua_State* GetState(::eyelib::scripting::ScriptHandle handle);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
