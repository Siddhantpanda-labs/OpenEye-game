#pragma once

#include "eyelib/world/Commands/CommandTypes.h"
#include "eyelib/world/components/Region.h"
#include "eyelib/world/components/RegionMembership.h"

#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace api {
namespace world {

using CommandPtr = std::unique_ptr<::eyelib::world::Command>;

CommandPtr CreateEntity(const std::string& name = "");
CommandPtr DestroyEntity(::eyelib::world::EntityId entity);
/** Reparent child under parent. Default preserveWorld = true (child stays in place, then follows parent). */
    CommandPtr ReparentEntity(::eyelib::world::EntityId child, ::eyelib::world::EntityId parent, bool preserveWorldTransform = true);
CommandPtr SetName(::eyelib::world::EntityId entity, const std::string& name);
CommandPtr AddTag(::eyelib::world::EntityId entity, const std::string& tag);
CommandPtr RemoveTag(::eyelib::world::EntityId entity, const std::string& tag);
CommandPtr SetMetadata(::eyelib::world::EntityId entity, const std::string& name, uint32_t flags = 0);
CommandPtr ClearMetadata(::eyelib::world::EntityId entity);
CommandPtr CreateRegion(const ::eyelib::world::components::Region& region, const std::string& name = "Region");
CommandPtr SetRegionData(::eyelib::world::EntityId entity, const ::eyelib::world::components::Region& region);
CommandPtr AttachRegionRules(::eyelib::world::EntityId entity, const std::vector<std::string>& ruleIds);
CommandPtr EnableSystemInRegion(::eyelib::world::EntityId entity, const std::string& systemName, bool enable);
CommandPtr AddRegionMembership(::eyelib::world::EntityId entity);

template<typename T>
CommandPtr AddComponent(::eyelib::world::EntityId entity, const T& value = T()) {
    return std::make_unique<::eyelib::world::AddComponentCommand<T>>(entity, value);
}

template<typename T>
CommandPtr RemoveComponent(::eyelib::world::EntityId entity) {
    return std::make_unique<::eyelib::world::RemoveComponentCommand<T>>(entity);
}

CommandPtr SetTransform(::eyelib::world::EntityId entity, const ::eyelib::world::components::Transform& value);
/** Set world transform; engine converts to local and stores. */
CommandPtr SetWorldTransform(::eyelib::world::EntityId entity, const ::eyelib::world::components::Transform& worldValue);

template<typename T>
CommandPtr SetComponentData(::eyelib::world::EntityId entity, const T& value) {
    return std::make_unique<::eyelib::world::SetComponentDataCommand<T>>(entity, value);
}

} // namespace world
} // namespace api
} // namespace eyelib
