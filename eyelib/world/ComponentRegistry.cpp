#include "ComponentRegistry.h"
#include "WorldState.h"

#include "eyelib/world/components/Name.h"
#include "eyelib/world/components/Transform.h"
#include "eyelib/world/components/Renderable.h"
#include "eyelib/world/components/Camera.h"
#include "eyelib/world/components/Light.h"
#include "eyelib/world/components/RigidBody.h"
#include "eyelib/world/components/Animator.h"
#include "eyelib/world/components/AABB.h"
#include "eyelib/world/components/MeshRenderer.h"
#include "eyelib/world/components/Collider.h"
#include "eyelib/world/components/Script.h"
#include "eyelib/world/components/Region.h"
#include "eyelib/world/components/SystemBindings.h"
#include "eyelib/world/components/RegionMembership.h"
#include "eyelib/world/components/ActiveSystems.h"

namespace eyelib {
namespace world {

ComponentRegistry& ComponentRegistry::Get() {
    static ComponentRegistry instance;
    return instance;
}

ComponentRegistry::ComponentRegistry() {
    RegisterBuiltins();
}

void ComponentRegistry::RegisterBuiltins() {
    Register<components::Name>("Name");
    Register<components::Transform>("Transform");
    Register<components::Renderable>("Renderable");
    Register<components::Camera>("Camera");
    Register<components::Light>("Light");
    Register<components::RigidBody>("RigidBody");
    Register<components::Animator>("Animator");
    Register<components::AABB>("AABB");
    Register<components::MeshRenderer>("MeshRenderer");
    Register<components::Collider>("Collider");
    Register<components::Script>("Script");
    Register<components::Region>("Region");
    Register<components::SystemBindings>("SystemBindings");
    Register<components::RegionMembership>("RegionMembership");
    Register<components::ActiveSystems>("ActiveSystems");
}

size_t ComponentRegistry::GetComponentSize(ComponentTypeId id) const {
    auto it = m_info.find(id);
    return it != m_info.end() ? it->second.size : 0;
}

size_t ComponentRegistry::GetComponentAlignment(ComponentTypeId id) const {
    auto it = m_info.find(id);
    return it != m_info.end() ? it->second.alignment : 0;
}

const std::string& ComponentRegistry::GetComponentName(ComponentTypeId id) const {
    auto it = m_info.find(id);
    static const std::string empty;
    return it != m_info.end() ? it->second.name : empty;
}

ComponentTypeId ComponentRegistry::FindComponentType(const std::string& name) const {
    auto it = m_nameToType.find(name);
    return it != m_nameToType.end() ? it->second : 0;
}

const ComponentTypeInfo* ComponentRegistry::GetTypeInfo(ComponentTypeId id) const {
    auto it = m_info.find(id);
    return it != m_info.end() ? &it->second : nullptr;
}

} // namespace world
} // namespace eyelib
