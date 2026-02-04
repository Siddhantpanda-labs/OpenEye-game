#pragma once

#include "eyelib/world/World.h"
#include "eyelib/world/observation/WorldObservation.h"
#include "World.h"

#include <optional>
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

    std::vector<World::TransformEntry> GetTransforms() const;
    std::vector<World::RenderableEntry> GetRenderables() const;
    std::vector<World::MeshRendererEntry> GetMeshRenderers() const;
    std::vector<World::CameraEntry> GetCameras() const;
    std::vector<World::LightEntry> GetLights() const;
    std::vector<World::RigidBodyEntry> GetRigidBodies() const;
    std::vector<World::ColliderEntry> GetColliders() const;

private:
    ::eyelib::world::WorldObservation m_observation;
};

} // namespace world
} // namespace api
} // namespace eyelib
