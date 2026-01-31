#include "ComponentSerializer.h"

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

ComponentSerializer& ComponentSerializer::Get() {
    static ComponentSerializer instance;
    return instance;
}

ComponentSerializer::ComponentSerializer() {
    RegisterBuiltins();
}

void ComponentSerializer::RegisterBuiltins() {
    Register<components::Name>("Name", 1);
    Register<components::Transform>("Transform", 1);
    Register<components::Renderable>("Renderable", 1);
    Register<components::Camera>("Camera", 1);
    Register<components::Light>("Light", 1);
    Register<components::RigidBody>("RigidBody", 1);
    Register<components::Animator>("Animator", 1);
    Register<components::AABB>("AABB", 1);
    Register<components::MeshRenderer>("MeshRenderer", 1);
    Register<components::Collider>("Collider", 1);
    Register<components::Script>("Script", 1);
    Register<components::Region>("Region", 1);
    Register<components::SystemBindings>("SystemBindings", 1);
    Register<components::RegionMembership>("RegionMembership", 1);
    Register<components::ActiveSystems>("ActiveSystems", 1);
}

const ComponentSerializer::Descriptor* ComponentSerializer::GetDescriptor(size_t typeKey) const {
    auto it = m_descriptors.find(typeKey);
    return it != m_descriptors.end() ? &it->second : nullptr;
}

const ComponentSerializer::Descriptor* ComponentSerializer::GetDescriptorByName(const std::string& name) const {
    auto it = m_nameToType.find(name);
    if (it == m_nameToType.end()) {
        return nullptr;
    }
    return GetDescriptor(it->second);
}

} // namespace world
} // namespace eyelib
