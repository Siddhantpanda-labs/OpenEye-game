#pragma once

#include <memory>
#include <string>

namespace eyelib {
namespace scripting {
    using ScriptHandle = size_t;
    constexpr ScriptHandle InvalidScriptHandle = 0;
}

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
    static scripting::ScriptHandle Load(const std::string& path);
    
    /**
     * @brief Unload a script.
     */
    static void Unload(scripting::ScriptHandle handle);
    
    /**
     * @brief Reload a script (hot-reload).
     * @return true if successful
     */
    static bool Reload(scripting::ScriptHandle handle);
    
    /**
     * @brief Get script path.
     */
    static std::string GetPath(scripting::ScriptHandle handle);
    
    /**
     * @brief Get script name (extracted from path).
     */
    static std::string GetName(scripting::ScriptHandle handle);
    
    /**
     * @brief Check if script is valid.
     */
    static bool IsValid(scripting::ScriptHandle handle);
    
    /**
     * @brief Get last error message for script.
     */
    static std::string GetError(scripting::ScriptHandle handle);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
