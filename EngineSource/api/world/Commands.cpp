#include "Commands.h"

namespace eyelib {
namespace api {
namespace world {

CommandPtr CreateEntity(const std::string& name) {
    return std::make_unique<::eyelib::world::CreateEntityCommand>(name);
}

CommandPtr DestroyEntity(::eyelib::world::EntityId entity) {
    return std::make_unique<::eyelib::world::DestroyEntityCommand>(entity);
}

CommandPtr ReparentEntity(::eyelib::world::EntityId child, ::eyelib::world::EntityId parent, bool preserveWorldTransform) {
    return std::make_unique<::eyelib::world::ReparentEntityCommand>(child, parent, preserveWorldTransform);
}

CommandPtr SetName(::eyelib::world::EntityId entity, const std::string& name) {
    return std::make_unique<::eyelib::world::SetNameCommand>(entity, name);
}

CommandPtr AddTag(::eyelib::world::EntityId entity, const std::string& tag) {
    return std::make_unique<::eyelib::world::AddTagCommand>(entity, tag);
}

CommandPtr RemoveTag(::eyelib::world::EntityId entity, const std::string& tag) {
    return std::make_unique<::eyelib::world::RemoveTagCommand>(entity, tag);
}

CommandPtr SetMetadata(::eyelib::world::EntityId entity, const std::string& name, uint32_t flags) {
    return std::make_unique<::eyelib::world::SetMetadataCommand>(entity, name, flags);
}

CommandPtr SetTransform(::eyelib::world::EntityId entity, const ::eyelib::world::components::Transform& value) {
    return std::make_unique<::eyelib::world::SetTransformCommand>(entity, value);
}

CommandPtr SetWorldTransform(::eyelib::world::EntityId entity, const ::eyelib::world::components::Transform& worldValue) {
    return std::make_unique<::eyelib::world::SetWorldTransformCommand>(entity, worldValue);
}

CommandPtr SetSkyboxAssetId(const std::string& assetId) {
    return std::make_unique<::eyelib::world::SetSkyboxAssetIdCommand>(assetId);
}

CommandPtr SetRenderPipelineMode(const std::string& mode) {
    return std::make_unique<::eyelib::world::SetRenderPipelineModeCommand>(mode);
}

CommandPtr ClearMetadata(::eyelib::world::EntityId entity) {
    return std::make_unique<::eyelib::world::ClearMetadataCommand>(entity);
}

CommandPtr CreateRegion(const ::eyelib::world::components::Region& region, const std::string& name) {
    return std::make_unique<::eyelib::world::CreateRegionCommand>(region, name);
}

CommandPtr SetRegionData(::eyelib::world::EntityId entity, const ::eyelib::world::components::Region& region) {
    return std::make_unique<::eyelib::world::SetRegionDataCommand>(entity, region);
}

CommandPtr AttachRegionRules(::eyelib::world::EntityId entity, const std::vector<std::string>& ruleIds) {
    return std::make_unique<::eyelib::world::AttachRegionRulesCommand>(entity, ruleIds);
}

CommandPtr EnableSystemInRegion(::eyelib::world::EntityId entity, const std::string& systemName, bool enable) {
    return std::make_unique<::eyelib::world::EnableSystemInRegionCommand>(entity, systemName, enable);
}

CommandPtr AddRegionMembership(::eyelib::world::EntityId entity) {
    return std::make_unique<::eyelib::world::AddComponentCommand<::eyelib::world::components::RegionMembership>>(entity);
}

} // namespace world
} // namespace api
} // namespace eyelib
