#include "Observation.h"

namespace eyelib {
namespace api {
namespace world {

Observation::Observation(const ::eyelib::world::World& world)
    : m_observation(world.GetView()) {}

std::vector<::eyelib::world::EntityId> Observation::GetEntities() const {
    return m_observation.GetEntities();
}

std::optional<std::string> Observation::GetEntityName(::eyelib::world::EntityId entity) const {
    return m_observation.GetEntityName(entity);
}

std::optional<eyelib::math::Transform> Observation::GetTransform(::eyelib::world::EntityId entity) const {
    return m_observation.GetTransform(entity);
}

std::vector<std::string> Observation::GetComponentNames(::eyelib::world::EntityId entity) const {
    return m_observation.GetComponentNames(entity);
}

std::vector<::eyelib::world::EntityId> Observation::GetRegionsContainingPoint(const eyelib::math::Vec3& point) const {
    return m_observation.GetRegionsContainingPoint(point);
}

std::optional<::eyelib::world::EntityId> Observation::GetHighestPriorityRegion(const eyelib::math::Vec3& point) const {
    return m_observation.GetHighestPriorityRegion(point);
}

std::span<const World::TransformEntry> Observation::GetTransformsView() const {
    const auto raw = m_observation.GetTransformsView();
    m_transformBuffer.clear();
    m_transformBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_transformBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::TransformEntry>(m_transformBuffer);
}

::eyelib::world::WorldObservation::TransformSoAView Observation::GetTransformSoAView() const {
    return m_observation.GetTransformSoAView();
}

std::span<const World::RenderableEntry> Observation::GetRenderablesView() const {
    const auto raw = m_observation.GetRenderablesView();
    m_renderableBuffer.clear();
    m_renderableBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_renderableBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::RenderableEntry>(m_renderableBuffer);
}

std::span<const World::MeshRendererEntry> Observation::GetMeshRenderersView() const {
    const auto raw = m_observation.GetMeshRenderersView();
    m_meshRendererBuffer.clear();
    m_meshRendererBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_meshRendererBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::MeshRendererEntry>(m_meshRendererBuffer);
}

std::span<const World::CameraEntry> Observation::GetCamerasView() const {
    const auto raw = m_observation.GetCamerasView();
    m_cameraBuffer.clear();
    m_cameraBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_cameraBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::CameraEntry>(m_cameraBuffer);
}

std::span<const World::LightEntry> Observation::GetLightsView() const {
    const auto raw = m_observation.GetLightsView();
    m_lightBuffer.clear();
    m_lightBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_lightBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::LightEntry>(m_lightBuffer);
}

std::span<const World::RigidBodyEntry> Observation::GetRigidBodiesView() const {
    const auto raw = m_observation.GetRigidBodiesView();
    m_rigidBodyBuffer.clear();
    m_rigidBodyBuffer.reserve(raw.size());
    for (const auto& p : raw) {
        m_rigidBodyBuffer.push_back({ p.first, p.second });
    }
    return std::span<const World::RigidBodyEntry>(m_rigidBodyBuffer);
}

std::span<const World::ColliderEntry> Observation::GetCollidersView() const {
    const auto raw = m_observation.GetCollidersView();
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
