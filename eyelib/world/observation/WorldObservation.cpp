#include "WorldObservation.h"

#include "eyelib/math/TransformUtils.h"
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

void WorldObservation::EnsureWorldTransformsComputed() const {
    if (!m_worldTransformCache.empty())
        return;
    const auto& store = m_view.GetTransformStore();
    for (const auto& [entity, ref] : store.All()) {
        const math::Mat4 world = ref.WorldMatrixRef();
        math::Vec3 pos, scl;
        math::Quat rot;
        math::Decompose(world, pos, rot, scl);
        m_worldTransformCache[entity] = components::Transform{ pos, rot, scl };
    }
}

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
    EnsureWorldTransformsComputed();
    auto it = m_worldTransformCache.find(entity);
    if (it != m_worldTransformCache.end())
        return math::Transform{ it->second.position, it->second.rotation, it->second.scale };
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
    EnsureWorldTransformsComputed();
    m_transformBuffer.clear();
    const auto& store = m_view.GetTransformStore();
    m_transformBuffer.reserve(store.Size());
    for (const auto& [entity, ref] : store.All()) {
        auto it = m_worldTransformCache.find(entity);
        if (it != m_worldTransformCache.end())
            m_transformBuffer.push_back({ entity, it->second });
    }
    return std::span<const std::pair<EntityId, components::Transform>>(m_transformBuffer);
}

WorldObservation::TransformSoAView WorldObservation::GetTransformSoAView() const {
    EnsureWorldTransformsComputed();
    m_soaEntityIds.clear();
    m_soaPositions.clear();
    m_soaRotations.clear();
    m_soaScales.clear();
    const auto& store = m_view.GetTransformStore();
    m_soaEntityIds.reserve(store.Size());
    m_soaPositions.reserve(store.Size());
    m_soaRotations.reserve(store.Size());
    m_soaScales.reserve(store.Size());
    for (const auto& [entity, ref] : store.All()) {
        auto it = m_worldTransformCache.find(entity);
        if (it != m_worldTransformCache.end()) {
            m_soaEntityIds.push_back(entity);
            m_soaPositions.push_back(it->second.position);
            m_soaRotations.push_back(it->second.rotation);
            m_soaScales.push_back(it->second.scale);
        }
    }
    return {
        m_soaEntityIds,
        m_soaPositions,
        m_soaRotations,
        m_soaScales,
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
