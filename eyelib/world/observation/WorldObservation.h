#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/Transform.h"
#include "eyelib/world/WorldView.h"
#include "eyelib/world/components/Camera.h"
#include "eyelib/world/components/Transform.h"
#include "eyelib/world/components/Collider.h"
#include "eyelib/world/components/Light.h"
#include "eyelib/world/components/MeshRenderer.h"
#include "eyelib/world/components/Renderable.h"
#include "eyelib/world/components/RigidBody.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace eyelib {
namespace world {

class WorldObservation {
public:
    explicit WorldObservation(WorldView view);

    WorldView GetView() const { return m_view; }

    std::vector<EntityId> GetEntities() const;
    std::optional<std::string> GetEntityName(EntityId entity) const;
    std::optional<math::Transform> GetTransform(EntityId entity) const;
    std::vector<std::string> GetComponentNames(EntityId entity) const;

    std::vector<EntityId> GetRegionsContainingPoint(const math::Vec3& point) const;
    std::optional<EntityId> GetHighestPriorityRegion(const math::Vec3& point) const;

    std::vector<std::pair<EntityId, components::Transform>> GetTransforms() const;
    std::vector<std::pair<EntityId, components::Renderable>> GetRenderables() const;
    std::vector<std::pair<EntityId, components::MeshRenderer>> GetMeshRenderers() const;
    std::vector<std::pair<EntityId, components::Camera>> GetCameras() const;
    std::vector<std::pair<EntityId, components::Light>> GetLights() const;
    std::vector<std::pair<EntityId, components::RigidBody>> GetRigidBodies() const;
    std::vector<std::pair<EntityId, components::Collider>> GetColliders() const;

private:
    WorldView m_view;
};

} // namespace world
} // namespace eyelib
