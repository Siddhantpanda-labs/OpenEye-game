#include "core/Engine.h"
#include "core/Log.h"
#include "core/Time.h"

namespace eyelib {
namespace core {

bool Engine::s_initialized = false;
bool Engine::s_shouldShutdown = false;
std::vector<std::unique_ptr<ISubsystem>> Engine::s_subsystems;

bool Engine::Initialize(const Config& config) {
    if (s_initialized) {
        Log::Warn("Engine already initialized");
        return true;
    }
    
    // Initialize logging first
    if (config.enableLogging) {
        Log::Initialize(config.logFile);
    }
    
    Log::Info("Initializing Eyelib Engine...");
    
    // Initialize time system
    Time::Initialize();
    
    // Initialize subsystems
    InitializeSubsystems();
    
    s_initialized = true;
    s_shouldShutdown = false;
    
    Log::Info("Engine initialized successfully");
    return true;
}

void Engine::Shutdown() {
    if (!s_initialized) {
        return;
    }
    
    Log::Info("Shutting down Eyelib Engine...");
    
    // Shutdown subsystems in reverse order
    ShutdownSubsystems();
    
    // Shutdown time system
    // (Time doesn't need explicit shutdown, but we could add it)
    
    // Shutdown logging last
    Log::Shutdown();
    
    s_initialized = false;
}

void Engine::RegisterSubsystem(std::unique_ptr<ISubsystem> subsystem) {
    if (!subsystem) {
        Log::Error("Attempted to register null subsystem");
        return;
    }
    
    s_subsystems.push_back(std::move(subsystem));
    Log::InfoF("Registered subsystem: %s", s_subsystems.back()->GetName());
}

void Engine::Run(
    std::function<void(float deltaTime)> updateCallback,
    std::function<void()> renderCallback
) {
    if (!s_initialized) {
        Log::Error("Engine not initialized. Call Engine::Initialize() first.");
        return;
    }
    
    if (!updateCallback) {
        Log::Error("Update callback is required");
        return;
    }
    
    Log::Info("Starting engine main loop...");
    
    while (!s_shouldShutdown) {
        // Update time
        Time::Update();
        
        float deltaTime = Time::DeltaTime();
        
        // Update logic
        updateCallback(deltaTime);
        
        // Render (if not headless and callback provided)
        if (renderCallback) {
            renderCallback();
        }
    }
    
    Log::Info("Engine main loop ended");
}

void Engine::InitializeSubsystems() {
    Log::Info("Initializing subsystems...");
    
    for (auto& subsystem : s_subsystems) {
        if (!subsystem) {
            continue;
        }
        
        Log::InfoF("Initializing subsystem: %s", subsystem->GetName());
        
        if (!subsystem->Initialize()) {
            Log::ErrorF("Failed to initialize subsystem: %s", subsystem->GetName());
            // Continue with other subsystems, but log the error
        } else {
            Log::InfoF("Subsystem initialized: %s", subsystem->GetName());
        }
    }
}

void Engine::ShutdownSubsystems() {
    Log::Info("Shutting down subsystems...");
    
    // Shutdown in reverse order
    for (auto it = s_subsystems.rbegin(); it != s_subsystems.rend(); ++it) {
        if (!*it) {
            continue;
        }
        
        Log::InfoF("Shutting down subsystem: %s", (*it)->GetName());
        (*it)->Shutdown();
    }
    
    s_subsystems.clear();
}

} // namespace core
} // namespace eyelib
