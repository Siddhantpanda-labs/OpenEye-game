#pragma once

#include <functional>
#include <memory>
#include <string>

namespace eyelib {
namespace api {

// Forward declaration for subsystem interface
class ISubsystem {
public:
    virtual ~ISubsystem() = default;
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual const char* GetName() const = 0;
};

/**
 * @brief Engine lifecycle and main loop management
 * 
 * This is the main entry point for client applications.
 * Provides engine initialization, subsystem registration, and main loop.
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
     */
    static void RegisterSubsystem(std::unique_ptr<ISubsystem> subsystem);
    
    /**
     * @brief Check if engine is initialized
     * @return true if initialized
     */
    static bool IsInitialized();
    
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
    static void RequestShutdown();
};

} // namespace api
} // namespace eyelib
