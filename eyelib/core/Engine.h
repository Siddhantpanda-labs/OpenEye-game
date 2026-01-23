#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <string>

namespace eyelib {
namespace core {

class Log;
class Time;

/**
 * @brief Engine subsystem interface
 * 
 * All engine subsystems must implement this interface
 * for proper initialization and shutdown ordering.
 */
class ISubsystem {
public:
    virtual ~ISubsystem() = default;
    
    /**
     * @brief Initialize the subsystem
     * @return true if successful, false otherwise
     */
    virtual bool Initialize() = 0;
    
    /**
     * @brief Shutdown the subsystem
     */
    virtual void Shutdown() = 0;
    
    /**
     * @brief Get subsystem name for logging
     * @return Subsystem name
     */
    virtual const char* GetName() const = 0;
};

/**
 * @brief Main engine class
 * 
 * Manages engine lifecycle, subsystem registration, and main loop.
 * Provides centralized initialization and shutdown.
 */
class Engine {
public:
    /**
     * @brief Engine configuration
     */
    struct Config {
        bool enableLogging = true;
        std::string logFile = "eyelib.log";
        bool headless = false;  // Run without rendering
    };
    
    /**
     * @brief Initialize the engine
     * @param config Engine configuration
     * @return true if successful, false otherwise
     */
    static bool Initialize(const Config& config = Config());
    
    /**
     * @brief Shutdown the engine
     */
    static void Shutdown();
    
    /**
     * @brief Register a subsystem
     * @param subsystem Subsystem to register
     * @note Subsystems are initialized in registration order
     *       and shutdown in reverse order
     */
    static void RegisterSubsystem(std::unique_ptr<ISubsystem> subsystem);
    
    /**
     * @brief Check if engine is initialized
     * @return true if initialized
     */
    static bool IsInitialized() { return s_initialized; }
    
    /**
     * @brief Run the main loop
     * @param updateCallback Function called each frame for update logic
     * @param renderCallback Function called each frame for render logic (optional if headless)
     */
    static void Run(
        std::function<void(float deltaTime)> updateCallback,
        std::function<void()> renderCallback = nullptr
    );
    
    /**
     * @brief Request engine to stop
     */
    static void RequestShutdown() { s_shouldShutdown = true; }

private:
    static bool s_initialized;
    static bool s_shouldShutdown;
    static std::vector<std::unique_ptr<ISubsystem>> s_subsystems;
    
    static void InitializeSubsystems();
    static void ShutdownSubsystems();
};

} // namespace core
} // namespace eyelib
