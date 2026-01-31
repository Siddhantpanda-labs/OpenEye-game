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

} // namespace world
} // namespace api
} // namespace eyelib
