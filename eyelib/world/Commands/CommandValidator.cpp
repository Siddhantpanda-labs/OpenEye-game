#include "CommandValidator.h"

#include "CommandTypes.h"
#include "eyelib/world/WorldGraph/Hierarchy.h"
#include "eyelib/world/Serialization/ComponentSerializer.h"

namespace eyelib {
namespace world {

bool CommandValidator::Validate(const WorldState& state, const Command& command) const {
    if (const auto* destroy = dynamic_cast<const DestroyEntityCommand*>(&command)) {
        return state.Entities().IsAlive(destroy->GetEntity());
    }
    if (const auto* setName = dynamic_cast<const SetNameCommand*>(&command)) {
        return state.Entities().IsAlive(setName->GetEntity());
    }
    if (const auto* setRegion = dynamic_cast<const SetRegionDataCommand*>(&command)) {
        return state.Entities().IsAlive(setRegion->GetEntity());
    }
    if (const auto* attachRules = dynamic_cast<const AttachRegionRulesCommand*>(&command)) {
        return state.Entities().IsAlive(attachRules->GetEntity());
    }
    if (const auto* enableSystem = dynamic_cast<const EnableSystemInRegionCommand*>(&command)) {
        return state.Entities().IsAlive(enableSystem->GetEntity());
    }
    if (const auto* comp = dynamic_cast<const ComponentCommandBase*>(&command)) {
        if (!state.Entities().IsAlive(comp->GetEntity())) {
            return false;
        }
        if (comp->GetComponentCommandKind() == ComponentCommandKind::Remove) {
            if (m_requiredComponents.find(comp->GetComponentTypeName()) != m_requiredComponents.end()) {
                return false;
            }
        }
        return true;
    }
    if (const auto* reparent = dynamic_cast<const ReparentEntityCommand*>(&command)) {
        if (!state.Entities().IsAlive(reparent->GetChild())) {
            return false;
        }
        if (!state.Entities().IsAlive(reparent->GetParent())) {
            return false;
        }
        return !state.GraphHierarchy().WouldCreateCycle(reparent->GetChild(), reparent->GetParent());
    }
    return true;
}

void CommandValidator::AddRequiredComponent(const std::string& componentName) {
    m_requiredComponents.insert(componentName);
}

void CommandValidator::ClearRequiredComponents() {
    m_requiredComponents.clear();
}

bool CommandValidator::HasRequiredComponents(const WorldState& state, EntityId entity) const {
    if (m_requiredComponents.empty()) {
        return true;
    }
    auto& serializer = ComponentSerializer::Get();
    for (const auto& name : m_requiredComponents) {
        const auto* desc = serializer.GetDescriptorByName(name);
        if (!desc) {
            continue;
        }
        auto* store = state.FindComponentStore(desc->typeKey);
        if (!store || !store->Has(entity)) {
            return false;
        }
    }
    return true;
}

} // namespace world
} // namespace eyelib
