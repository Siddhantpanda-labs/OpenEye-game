#include "WorldObservation.h"

#include "eyelib/world/ComponentStorage/TransformStore.h"
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
    const auto& store = m_view.GetTransformStore();
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

std::span<const std::pair<EntityId, components::Transform>> WorldObservation::GetTransformsView() const {
    m_transformBuffer.clear();
    const auto& store = m_view.GetTransformStore();
    const auto range = store.All();
    m_transformBuffer.reserve(store.Size());
    for (const auto& [entity, ref] : range) {
        m_transformBuffer.push_back({ entity, static_cast<math::Transform>(ref) });
    }
    return std::span<const std::pair<EntityId, components::Transform>>(m_transformBuffer);
}

WorldObservation::TransformSoAView WorldObservation::GetTransformSoAView() const {
    const auto& store = m_view.GetTransformStore();
    return {
        store.EntitiesSpan(),
        store.PositionsSpan(),
        store.RotationsSpan(),
        store.ScalesSpan(),
    };
}

std::span<const std::pair<EntityId, components::Renderable>> WorldObservation::GetRenderablesView() const {
    m_renderableBuffer.clear();
    const auto& store = m_view.Components<components::Renderable>().All();
    m_renderableBuffer.reserve(store.size());
    for (const auto& [entity, component] : store) {
        m_renderableBuffer.push_back({ entity, component });
    }
    return std::span<const std::pair<EntityId, components::Renderable>>(m_renderableBuffer);
}

std::span<const std::pair<EntityId, components::MeshRenderer>> WorldObservation::GetMeshRenderersView() const {
    m_meshRendererBuffer.clear();
    const auto& store = m_view.Components<components::MeshRenderer>().All();
    m_meshRendererBuffer.reserve(store.size());
    for (const auto& [entity, component] : store) {
        m_meshRendererBuffer.push_back({ entity, component });
    }
    return std::span<const std::pair<EntityId, components::MeshRenderer>>(m_meshRendererBuffer);
}

std::span<const std::pair<EntityId, components::Camera>> WorldObservation::GetCamerasView() const {
    m_cameraBuffer.clear();
    const auto& store = m_view.Components<components::Camera>().All();
    m_cameraBuffer.reserve(store.size());
    for (const auto& [entity, component] : store) {
        m_cameraBuffer.push_back({ entity, component });
    }
    return std::span<const std::pair<EntityId, components::Camera>>(m_cameraBuffer);
}

std::span<const std::pair<EntityId, components::Light>> WorldObservation::GetLightsView() const {
    m_lightBuffer.clear();
    const auto& store = m_view.Components<components::Light>().All();
    m_lightBuffer.reserve(store.size());
    for (const auto& [entity, component] : store) {
        m_lightBuffer.push_back({ entity, component });
    }
    return std::span<const std::pair<EntityId, components::Light>>(m_lightBuffer);
}

std::span<const std::pair<EntityId, components::RigidBody>> WorldObservation::GetRigidBodiesView() const {
    m_rigidBodyBuffer.clear();
    const auto& store = m_view.Components<components::RigidBody>().All();
    m_rigidBodyBuffer.reserve(store.size());
    for (const auto& [entity, component] : store) {
        m_rigidBodyBuffer.push_back({ entity, component });
    }
    return std::span<const std::pair<EntityId, components::RigidBody>>(m_rigidBodyBuffer);
}

std::span<const std::pair<EntityId, components::Collider>> WorldObservation::GetCollidersView() const {
    m_colliderBuffer.clear();
    const auto& store = m_view.Components<components::Collider>().All();
    m_colliderBuffer.reserve(store.size());
    for (const auto& [entity, component] : store) {
        m_colliderBuffer.push_back({ entity, component });
    }
    return std::span<const std::pair<EntityId, components::Collider>>(m_colliderBuffer);
}

} // namespace world
} // namespace eyelib
