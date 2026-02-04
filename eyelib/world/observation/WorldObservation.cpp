#include "WorldObservation.h"

#include "eyelib/world/RegionQueries.h"
#include "eyelib/world/Serialization/ComponentSerializer.h"
#include "eyelib/world/EntityRegistry.h"
#include "eyelib/world/WorldGraph/Metadata.h"
#include "eyelib/world/components/Camera.h"
#include "eyelib/world/components/Collider.h"
#include "eyelib/world/components/Light.h"
#include "eyelib/world/components/MeshRenderer.h"
#include "eyelib/world/components/Renderable.h"
#include "eyelib/world/components/RigidBody.h"
#include "eyelib/world/components/Transform.h"

namespace eyelib {
namespace world {

WorldObservation::WorldObservation(WorldView view)
    : m_view(std::move(view)) {}

std::vector<EntityId> WorldObservation::GetEntities() const {
    return m_view.Entities().GetAliveEntities();
}

std::optional<std::string> WorldObservation::GetEntityName(EntityId entity) const {
    auto entry = m_view.GraphMetadata().Get(entity);
    if (!entry.has_value()) {
        return std::nullopt;
    }
    return entry->name;
}

std::optional<math::Transform> WorldObservation::GetTransform(EntityId entity) const {
    const auto& store = m_view.Components<components::Transform>();
    if (const auto* t = store.TryGet(entity)) {
        return *t;
    }
    return std::nullopt;
}

std::vector<std::string> WorldObservation::GetComponentNames(EntityId entity) const {
    std::vector<std::string> names;
    const auto& stores = m_view.ComponentStores();
    auto& serializer = ComponentSerializer::Get();
    for (const auto& [typeKey, store] : stores) {
        if (!store || !store->Has(entity)) {
            continue;
        }
        const auto* desc = serializer.GetDescriptor(typeKey);
        if (desc && !desc->name.empty()) {
            names.push_back(desc->name);
        }
    }
    return names;
}

std::vector<EntityId> WorldObservation::GetRegionsContainingPoint(const math::Vec3& point) const {
    return world::GetRegionsContainingPoint(m_view.State(), point);
}

std::optional<EntityId> WorldObservation::GetHighestPriorityRegion(const math::Vec3& point) const {
    return world::GetHighestPriorityRegion(m_view.State(), point);
}

std::vector<std::pair<EntityId, components::Transform>> WorldObservation::GetTransforms() const {
    std::vector<std::pair<EntityId, components::Transform>> out;
    const auto& store = m_view.Components<components::Transform>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<std::pair<EntityId, components::Renderable>> WorldObservation::GetRenderables() const {
    std::vector<std::pair<EntityId, components::Renderable>> out;
    const auto& store = m_view.Components<components::Renderable>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<std::pair<EntityId, components::MeshRenderer>> WorldObservation::GetMeshRenderers() const {
    std::vector<std::pair<EntityId, components::MeshRenderer>> out;
    const auto& store = m_view.Components<components::MeshRenderer>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<std::pair<EntityId, components::Camera>> WorldObservation::GetCameras() const {
    std::vector<std::pair<EntityId, components::Camera>> out;
    const auto& store = m_view.Components<components::Camera>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<std::pair<EntityId, components::Light>> WorldObservation::GetLights() const {
    std::vector<std::pair<EntityId, components::Light>> out;
    const auto& store = m_view.Components<components::Light>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<std::pair<EntityId, components::RigidBody>> WorldObservation::GetRigidBodies() const {
    std::vector<std::pair<EntityId, components::RigidBody>> out;
    const auto& store = m_view.Components<components::RigidBody>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

std::vector<std::pair<EntityId, components::Collider>> WorldObservation::GetColliders() const {
    std::vector<std::pair<EntityId, components::Collider>> out;
    const auto& store = m_view.Components<components::Collider>().All();
    out.reserve(store.size());
    for (const auto& [entity, component] : store) {
        out.push_back({ entity, component });
    }
    return out;
}

} // namespace world
} // namespace eyelib
