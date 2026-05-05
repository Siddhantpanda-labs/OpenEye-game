#include "Engine.h"
#include "eyelib/core/Engine.h"

namespace eyelib {
namespace api {

bool Engine::Initialize(const Config& config) {
    eyelib::core::Engine::Config coreConfig;
    coreConfig.enableLogging = config.enableLogging;
    coreConfig.logFile = config.logFile;
    coreConfig.headless = config.headless;
    
    return eyelib::core::Engine::Initialize(coreConfig);
}

void Engine::Shutdown() {
    eyelib::core::Engine::Shutdown();
}

void Engine::RegisterSubsystem(std::unique_ptr<ISubsystem> subsystem) {
    // Convert API subsystem to core subsystem
    // For now, we'll create a wrapper adapter
    // This is a simplified version - full implementation would need proper adapter pattern
    if (!subsystem) {
        return;
    }
    
    // Create a core subsystem wrapper
    class SubsystemAdapter : public eyelib::core::ISubsystem {
    public:
        SubsystemAdapter(std::unique_ptr<eyelib::api::ISubsystem> apiSubsystem)
            : m_apiSubsystem(std::move(apiSubsystem)) {}
        
        bool Initialize() override {
            return m_apiSubsystem->Initialize();
        }
        
        void Shutdown() override {
            m_apiSubsystem->Shutdown();
        }
        
        const char* GetName() const override {
            return m_apiSubsystem->GetName();
        }
        
    private:
        std::unique_ptr<eyelib::api::ISubsystem> m_apiSubsystem;
    };
    
    auto adapter = std::make_unique<SubsystemAdapter>(std::move(subsystem));
    eyelib::core::Engine::RegisterSubsystem(std::move(adapter));
}

bool Engine::IsInitialized() {
    return eyelib::core::Engine::IsInitialized();
}

void Engine::Run(
    std::function<void(float deltaTime)> updateCallback,
    std::function<void()> renderCallback
) {
    eyelib::core::Engine::Run(updateCallback, renderCallback);
}

void Engine::RequestShutdown() {
    eyelib::core::Engine::RequestShutdown();
}

} // namespace api
} // namespace eyelib
