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

std::vector<World::TransformEntry> Observation::GetTransforms() const {
    auto raw = m_observation.GetTransforms();
    std::vector<World::TransformEntry> out;
    out.reserve(raw.size());
    for (const auto& p : raw) {
        out.push_back({ p.first, p.second });
    }
    return out;
}

std::vector<World::RenderableEntry> Observation::GetRenderables() const {
    auto raw = m_observation.GetRenderables();
    std::vector<World::RenderableEntry> out;
    out.reserve(raw.size());
    for (const auto& p : raw) {
        out.push_back({ p.first, p.second });
    }
    return out;
}

std::vector<World::MeshRendererEntry> Observation::GetMeshRenderers() const {
    auto raw = m_observation.GetMeshRenderers();
    std::vector<World::MeshRendererEntry> out;
    out.reserve(raw.size());
    for (const auto& p : raw) {
        out.push_back({ p.first, p.second });
    }
    return out;
}

std::vector<World::CameraEntry> Observation::GetCameras() const {
    auto raw = m_observation.GetCameras();
    std::vector<World::CameraEntry> out;
    out.reserve(raw.size());
    for (const auto& p : raw) {
        out.push_back({ p.first, p.second });
    }
    return out;
}

std::vector<World::LightEntry> Observation::GetLights() const {
    auto raw = m_observation.GetLights();
    std::vector<World::LightEntry> out;
    out.reserve(raw.size());
    for (const auto& p : raw) {
        out.push_back({ p.first, p.second });
    }
    return out;
}

std::vector<World::RigidBodyEntry> Observation::GetRigidBodies() const {
    auto raw = m_observation.GetRigidBodies();
    std::vector<World::RigidBodyEntry> out;
    out.reserve(raw.size());
    for (const auto& p : raw) {
        out.push_back({ p.first, p.second });
    }
    return out;
}

std::vector<World::ColliderEntry> Observation::GetColliders() const {
    auto raw = m_observation.GetColliders();
    std::vector<World::ColliderEntry> out;
    out.reserve(raw.size());
    for (const auto& p : raw) {
        out.push_back({ p.first, p.second });
    }
    return out;
}

} // namespace world
} // namespace api
} // namespace eyelib
