#pragma once

#include <memory>
#include <string>
#include <functional>

namespace eyelib {
namespace api {

/**
 * @brief Lua VM wrapper - manages a Lua virtual machine with sandboxing.
 * Uses PIMPL to hide implementation details.
 */
class ScriptVM {
public:
    enum class SandboxMode {
        Restricted,  // Default: only engine APIs
        Standard,    // Standard Lua libraries
        Permissive   // Almost everything (dev only)
    };

    ScriptVM();
    ~ScriptVM();

    ScriptVM(const ScriptVM&) = delete;
    ScriptVM& operator=(const ScriptVM&) = delete;
    ScriptVM(ScriptVM&& other) noexcept;
    ScriptVM& operator=(ScriptVM&& other) noexcept;

    /**
     * @brief Create a new VM instance.
     */
    static std::shared_ptr<ScriptVM> Create();
    
    /**
     * @brief Check if VM is valid.
     */
    bool IsValid() const;
    
    /**
     * @brief Set sandbox mode.
     */
    void SetSandboxMode(SandboxMode mode);
    
    /**
     * @brief Execute Lua code string.
     * @return true if successful, error message in error parameter
     */
    bool ExecuteString(const std::string& code, std::string& error);
    
    /**
     * @brief Execute Lua file.
     * @return true if successful, error message in error parameter
     */
    bool ExecuteFile(const std::string& path, std::string& error);
    
    /**
     * @brief Set memory limit (0 = unlimited).
     */
    void SetMemoryLimit(size_t bytes);
    
    /**
     * @brief Get current memory usage.
     */
    size_t GetMemoryUsage() const;
    
    /**
     * @brief Set error callback.
     */
    void SetErrorCallback(std::function<void(const std::string&)> callback);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
