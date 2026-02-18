#include "Observation.h"

namespace eyelib {
namespace api {
namespace world {

Observation::Observation(const ::eyelib::world::World& world)
    : m_observation(std::in_place, world.GetView())
    , m_snapshot(nullptr) {}

Observation::Observation(const World::RenderSnapshot& snapshot)
    : m_observation(std::nullopt)
    , m_snapshot(&snapshot) {}

std::vector<::eyelib::world::EntityId> Observation::GetEntities() const {
    if (m_snapshot) {
        std::vector<::eyelib::world::EntityId> out;
        out.reserve(m_snapshot->transformCache.size());
        for (const auto& [e, t] : m_snapshot->transformCache) {
            (void)t;
            out.push_back(e);
        }
        return out;
    }
    return m_observation->GetEntities();
}

std::optional<std::string> Observation::GetEntityName(::eyelib::world::EntityId entity) const {
    if (m_snapshot) return std::nullopt;
    return m_observation->GetEntityName(entity);
}

std::optional<eyelib::math::Transform> Observation::GetTransform(::eyelib::world::EntityId entity) const {
    if (m_snapshot) {
        auto it = m_snapshot->transformCache.find(entity);
        if (it != m_snapshot->transformCache.end())
            return eyelib::math::Transform{ it->second.position, it->second.rotation, it->second.scale };
        return std::nullopt;
    }
    return m_observation->GetTransform(entity);
}

std::vector<std::string> Observation::GetComponentNames(::eyelib::world::EntityId entity) const {
    if (m_snapshot) return {};
    return m_observation->GetComponentNames(entity);
}

std::vector<::eyelib::world::EntityId> Observation::GetRegionsContainingPoint(const eyelib::math::Vec3& point) const {
    if (m_snapshot) return {};
    return m_observation->GetRegionsContainingPoint(point);
}

std::optional<::eyelib::world::EntityId> Observation::GetHighestPriorityRegion(const eyelib::math::Vec3& point) const {
    if (m_snapshot) return std::nullopt;
    return m_observation->GetHighestPriorityRegion(point);
}

std::span<const World::TransformEntry> Observation::GetTransformsView() const {
    if (m_snapshot)
        return std::span<const World::TransformEntry>(m_snapshot->transforms);
    const auto raw = m_observation->GetTransformsView();
    m_transformBuffer.clear();
    m_transformBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_transformBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::TransformEntry>(m_transformBuffer);
}

::eyelib::world::WorldObservation::TransformSoAView Observation::GetTransformSoAView() const {
    if (m_snapshot) {
        return {
            std::span<const ::eyelib::world::EntityId>(m_snapshot->transformEntityIds),
            std::span<const ::eyelib::math::Vec3>(m_snapshot->transformPositions),
            std::span<const ::eyelib::math::Quat>(m_snapshot->transformRotations),
            std::span<const ::eyelib::math::Vec3>(m_snapshot->transformScales),
        };
    }
    return m_observation->GetTransformSoAView();
}

std::span<const World::RenderableEntry> Observation::GetRenderablesView() const {
    if (m_snapshot) return std::span<const World::RenderableEntry>(m_snapshot->renderables);
    const auto raw = m_observation->GetRenderablesView();
    m_renderableBuffer.clear();
    m_renderableBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_renderableBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::RenderableEntry>(m_renderableBuffer);
}

std::span<const World::MeshRendererEntry> Observation::GetMeshRenderersView() const {
    if (m_snapshot) return std::span<const World::MeshRendererEntry>(m_snapshot->meshRenderers);
    const auto raw = m_observation->GetMeshRenderersView();
    m_meshRendererBuffer.clear();
    m_meshRendererBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_meshRendererBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::MeshRendererEntry>(m_meshRendererBuffer);
}

std::span<const World::CameraEntry> Observation::GetCamerasView() const {
    if (m_snapshot) return std::span<const World::CameraEntry>(m_snapshot->cameras);
    const auto raw = m_observation->GetCamerasView();
    m_cameraBuffer.clear();
    m_cameraBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_cameraBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::CameraEntry>(m_cameraBuffer);
}

std::span<const World::LightEntry> Observation::GetLightsView() const {
    if (m_snapshot) return std::span<const World::LightEntry>(m_snapshot->lights);
    const auto raw = m_observation->GetLightsView();
    m_lightBuffer.clear();
    m_lightBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_lightBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::LightEntry>(m_lightBuffer);
}

std::span<const World::RigidBodyEntry> Observation::GetRigidBodiesView() const {
    if (m_snapshot) return std::span<const World::RigidBodyEntry>(m_snapshot->rigidBodies);
    const auto raw = m_observation->GetRigidBodiesView();
    m_rigidBodyBuffer.clear();
    m_rigidBodyBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_rigidBodyBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::RigidBodyEntry>(m_rigidBodyBuffer);
}

std::span<const World::ColliderEntry> Observation::GetCollidersView() const {
    if (m_snapshot) return std::span<const World::ColliderEntry>(m_snapshot->colliders);
    const auto raw = m_observation->GetCollidersView();
    m_colliderBuffer.clear();
    m_colliderBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_colliderBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::ColliderEntry>(m_colliderBuffer);
}

} // namespace world
} // namespace api
} // namespace eyelib
