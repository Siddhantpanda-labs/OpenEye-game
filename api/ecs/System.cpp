#include "System.h"

namespace eyelib {
namespace api {

class SystemManager::Impl {
public:
    eyelib::ecs::SystemManager manager;
};

SystemManager::SystemManager() : m_impl(std::make_unique<Impl>()) {}
SystemManager::~SystemManager() = default;

void SystemManager::RegisterSystem(std::shared_ptr<eyelib::ecs::ISystem> system) {
    m_impl->manager.RegisterSystem(std::move(system));
}

void SystemManager::UpdateAll(World& world, float deltaTime) {
    m_impl->manager.UpdateAll(*world.GetInternal(), deltaTime);
}

} // namespace api
} // namespace eyelib
