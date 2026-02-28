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
#include "eyelib/world/EntityId.h"

#include <optional>
#include <span>
#include <string>
#include <unordered_map>
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

    /** Zero-copy view over transforms. Valid until next GetXxxView() or world mutation. */
    std::span<const std::pair<EntityId, components::Transform>> GetTransformsView() const;

    /** SoA view over transform data (zero-copy). Valid until world mutation. */
    struct TransformSoAView {
        std::span<const EntityId> entities;
        std::span<const math::Vec3> positions;
        std::span<const math::Quat> rotations;
        std::span<const math::Vec3> scales;
    };
    TransformSoAView GetTransformSoAView() const;
    std::span<const std::pair<EntityId, components::Renderable>> GetRenderablesView() const;
    std::span<const std::pair<EntityId, components::MeshRenderer>> GetMeshRenderersView() const;
    std::span<const std::pair<EntityId, components::Camera>> GetCamerasView() const;
    std::span<const std::pair<EntityId, components::Light>> GetLightsView() const;
    std::span<const std::pair<EntityId, components::RigidBody>> GetRigidBodiesView() const;
    std::span<const std::pair<EntityId, components::Collider>> GetCollidersView() const;

private:
    void EnsureWorldTransformsComputed() const;

    WorldView m_view;
    mutable std::unordered_map<EntityId, components::Transform, EntityIdHash> m_worldTransformCache;
    mutable std::vector<std::pair<EntityId, components::Transform>> m_transformBuffer;
    mutable std::vector<EntityId> m_soaEntityIds;
    mutable std::vector<math::Vec3> m_soaPositions;
    mutable std::vector<math::Quat> m_soaRotations;
    mutable std::vector<math::Vec3> m_soaScales;
    mutable std::vector<std::pair<EntityId, components::Renderable>> m_renderableBuffer;
    mutable std::vector<std::pair<EntityId, components::MeshRenderer>> m_meshRendererBuffer;
    mutable std::vector<std::pair<EntityId, components::Camera>> m_cameraBuffer;
    mutable std::vector<std::pair<EntityId, components::Light>> m_lightBuffer;
    mutable std::vector<std::pair<EntityId, components::RigidBody>> m_rigidBodyBuffer;
    mutable std::vector<std::pair<EntityId, components::Collider>> m_colliderBuffer;
};

} // namespace world
} // namespace eyelib
