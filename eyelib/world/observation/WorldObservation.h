#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/math/Transform.h"
#include "eyelib/world/WorldView.h"

#include <optional>
#include <string>
#include <vector>

namespace eyelib {
namespace world {

class WorldObservation {
public:
    explicit WorldObservation(WorldView view);

    std::vector<EntityId> GetEntities() const;
    std::optional<std::string> GetEntityName(EntityId entity) const;
    std::optional<math::Transform> GetTransform(EntityId entity) const;
    std::vector<std::string> GetComponentNames(EntityId entity) const;

    std::vector<EntityId> GetRegionsContainingPoint(const math::Vec3& point) const;
    std::optional<EntityId> GetHighestPriorityRegion(const math::Vec3& point) const;

private:
    WorldView m_view;
};

} // namespace world
} // namespace eyelib
