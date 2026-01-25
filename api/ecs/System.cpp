#include "System.h"

#include "eyelib/ecs/systems/AnimationSystem.h"
#include "eyelib/ecs/systems/AudioSystem.h"
#include "eyelib/ecs/systems/CameraSystem.h"
#include "eyelib/ecs/systems/CullingSystem.h"
#include "eyelib/ecs/systems/LightSystem.h"
#include "eyelib/ecs/systems/PhysicsSystem.h"
#include "eyelib/ecs/systems/RenderSystem.h"
#include "eyelib/ecs/systems/TransformSystem.h"

namespace eyelib {
namespace api {

class SystemManager::Impl {
public:
    eyelib::ecs::SystemManager manager;
};

SystemManager::SystemManager() : m_impl(std::make_unique<Impl>()) {
    m_impl->manager.RegisterSystem(std::make_shared<eyelib::ecs::TransformSystem>());
    m_impl->manager.RegisterSystem(std::make_shared<eyelib::ecs::CameraSystem>());
    m_impl->manager.RegisterSystem(std::make_shared<eyelib::ecs::LightSystem>());
    m_impl->manager.RegisterSystem(std::make_shared<eyelib::ecs::AnimationSystem>());
    m_impl->manager.RegisterSystem(std::make_shared<eyelib::ecs::PhysicsSystem>());
    m_impl->manager.RegisterSystem(std::make_shared<eyelib::ecs::AudioSystem>());
    m_impl->manager.RegisterSystem(std::make_shared<eyelib::ecs::CullingSystem>());
    m_impl->manager.RegisterSystem(std::make_shared<eyelib::ecs::RenderSystem>());
}
SystemManager::~SystemManager() = default;

void SystemManager::RegisterSystem(std::shared_ptr<eyelib::ecs::ISystem> system) {
    m_impl->manager.RegisterSystem(std::move(system));
}

void SystemManager::UpdateAll(World& world, float deltaTime) {
    m_impl->manager.UpdateAll(*world.GetInternal(), deltaTime);
}

} // namespace api
} // namespace eyelib
