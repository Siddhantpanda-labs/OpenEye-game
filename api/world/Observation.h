#pragma once

#include "eyelib/world/World.h"
#include "eyelib/world/observation/WorldObservation.h"
#include "World.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace eyelib {
namespace api {
namespace world {

class Observation {
public:
    explicit Observation(const ::eyelib::world::World& world);

    std::vector<::eyelib::world::EntityId> GetEntities() const;
    std::optional<std::string> GetEntityName(::eyelib::world::EntityId entity) const;
    std::optional<eyelib::math::Transform> GetTransform(::eyelib::world::EntityId entity) const;
    std::vector<std::string> GetComponentNames(::eyelib::world::EntityId entity) const;

    std::vector<::eyelib::world::EntityId> GetRegionsContainingPoint(const eyelib::math::Vec3& point) const;
    std::optional<::eyelib::world::EntityId> GetHighestPriorityRegion(const eyelib::math::Vec3& point) const;

    /** Zero-copy view. Valid until next GetXxxView() or world mutation. */
    std::span<const World::TransformEntry> GetTransformsView() const;

    /** Zero-copy SoA view over transform arrays. Valid until world mutation. */
    ::eyelib::world::WorldObservation::TransformSoAView GetTransformSoAView() const;
    std::span<const World::RenderableEntry> GetRenderablesView() const;
    std::span<const World::MeshRendererEntry> GetMeshRenderersView() const;
    std::span<const World::CameraEntry> GetCamerasView() const;
    std::span<const World::LightEntry> GetLightsView() const;
    std::span<const World::RigidBodyEntry> GetRigidBodiesView() const;
    std::span<const World::ColliderEntry> GetCollidersView() const;

private:
    ::eyelib::world::WorldObservation m_observation;
    mutable std::vector<World::TransformEntry> m_transformBuffer;
    mutable std::vector<World::RenderableEntry> m_renderableBuffer;
    mutable std::vector<World::MeshRendererEntry> m_meshRendererBuffer;
    mutable std::vector<World::CameraEntry> m_cameraBuffer;
    mutable std::vector<World::LightEntry> m_lightBuffer;
    mutable std::vector<World::RigidBodyEntry> m_rigidBodyBuffer;
    mutable std::vector<World::ColliderEntry> m_colliderBuffer;
};

} // namespace world
} // namespace api
} // namespace eyelib
