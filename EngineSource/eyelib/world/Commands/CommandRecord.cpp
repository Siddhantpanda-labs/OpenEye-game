#include "CommandRecord.h"

#include "CommandTypes.h"
#include "eyelib/serialize/SerializationTraits.h"
#include "eyelib/world/components/Region.h"

namespace eyelib {
namespace world {

namespace {
static nlohmann::json EncodeEntity(const EntityId& id) {
    return { {"index", id.index}, {"generation", id.generation} };
}

static EntityId DecodeEntity(const nlohmann::json& j) {
    EntityId id;
    id.index = j.value("index", 0u);
    id.generation = j.value("generation", 0u);
    return id;
}
} // namespace

std::optional<CommandRecord> CommandCodec::Encode(const Command& command) {
    if (const auto* comp = dynamic_cast<const ComponentCommandBase*>(&command)) {
        CommandRecord rec;
        rec.type = "ComponentCommand";
        rec.payload["entity"] = EncodeEntity(comp->GetEntity());
        rec.payload["component"] = comp->GetComponentTypeName();
        switch (comp->GetComponentCommandKind()) {
        case ComponentCommandKind::Add:
            rec.payload["kind"] = "Add";
            break;
        case ComponentCommandKind::Remove:
            rec.payload["kind"] = "Remove";
            break;
        case ComponentCommandKind::Set:
            rec.payload["kind"] = "Set";
            break;
        }
        rec.payload["data"] = comp->SerializeData();
        return rec;
    }
    if (const auto* create = dynamic_cast<const CreateEntityCommand*>(&command)) {
        CommandRecord rec;
        rec.type = "CreateEntity";
        rec.payload["name"] = create->GetEntityName();
        return rec;
    }
    if (const auto* destroy = dynamic_cast<const DestroyEntityCommand*>(&command)) {
        CommandRecord rec;
        rec.type = "DestroyEntity";
        rec.payload["entity"] = EncodeEntity(destroy->GetEntity());
        return rec;
    }
    if (const auto* reparent = dynamic_cast<const ReparentEntityCommand*>(&command)) {
        CommandRecord rec;
        rec.type = "ReparentEntity";
        rec.payload["child"] = EncodeEntity(reparent->GetChild());
        rec.payload["parent"] = EncodeEntity(reparent->GetParent());
        return rec;
    }
    if (const auto* setName = dynamic_cast<const SetNameCommand*>(&command)) {
        CommandRecord rec;
        rec.type = "SetName";
        rec.payload["entity"] = EncodeEntity(setName->GetEntity());
        rec.payload["name"] = setName->GetNewName();
        return rec;
    }
    if (const auto* createRegion = dynamic_cast<const CreateRegionCommand*>(&command)) {
        CommandRecord rec;
        rec.type = "CreateRegion";
        rec.payload["name"] = createRegion->GetRegionName();
        rec.payload["region"] = eyelib::serialize::SerializationTraits<components::Region>::Serialize(createRegion->GetRegionData());
        return rec;
    }
    if (const auto* setRegion = dynamic_cast<const SetRegionDataCommand*>(&command)) {
        CommandRecord rec;
        rec.type = "SetRegionData";
        rec.payload["entity"] = EncodeEntity(setRegion->GetEntity());
        rec.payload["region"] = eyelib::serialize::SerializationTraits<components::Region>::Serialize(setRegion->GetRegionData());
        return rec;
    }
    if (const auto* attachRules = dynamic_cast<const AttachRegionRulesCommand*>(&command)) {
        CommandRecord rec;
        rec.type = "AttachRegionRules";
        rec.payload["entity"] = EncodeEntity(attachRules->GetEntity());
        rec.payload["ruleIds"] = eyelib::serialize::SerializationTraits<std::vector<std::string>>::Serialize(attachRules->GetRuleIds());
        return rec;
    }
    if (const auto* enableSystem = dynamic_cast<const EnableSystemInRegionCommand*>(&command)) {
        CommandRecord rec;
        rec.type = "EnableSystemInRegion";
        rec.payload["entity"] = EncodeEntity(enableSystem->GetEntity());
        rec.payload["systemName"] = enableSystem->GetSystemName();
        rec.payload["enable"] = enableSystem->GetEnable();
        return rec;
    }
    return std::nullopt;
}

std::unique_ptr<Command> CommandCodec::Decode(const CommandRecord& record) {
    if (record.type == "ComponentCommand") {
        const auto entity = DecodeEntity(record.payload["entity"]);
        const std::string typeName = record.payload.value("component", std::string{});
        const std::string kind = record.payload.value("kind", std::string{});
        const auto data = record.payload.value("data", nlohmann::json{});

        if (kind == "Add") {
            return std::make_unique<AddComponentByNameCommand>(entity, typeName, data);
        }
        if (kind == "Remove") {
            return std::make_unique<RemoveComponentByNameCommand>(entity, typeName);
        }
        if (kind == "Set") {
            return std::make_unique<SetComponentDataByNameCommand>(entity, typeName, data);
        }
        return nullptr;
    }
    if (record.type == "CreateEntity") {
        const std::string name = record.payload.value("name", std::string{});
        return std::make_unique<CreateEntityCommand>(name);
    }
    if (record.type == "DestroyEntity") {
        return std::make_unique<DestroyEntityCommand>(DecodeEntity(record.payload["entity"]));
    }
    if (record.type == "ReparentEntity") {
        return std::make_unique<ReparentEntityCommand>(
            DecodeEntity(record.payload["child"]),
            DecodeEntity(record.payload["parent"])
        );
    }
    if (record.type == "SetName") {
        return std::make_unique<SetNameCommand>(
            DecodeEntity(record.payload["entity"]),
            record.payload.value("name", std::string{})
        );
    }
    if (record.type == "CreateRegion") {
        const std::string name = record.payload.value("name", std::string{"Region"});
        components::Region region = eyelib::serialize::SerializationTraits<components::Region>::Deserialize(
            record.payload.value("region", nlohmann::json{}), components::Region{}
        );
        return std::make_unique<CreateRegionCommand>(region, name);
    }
    if (record.type == "SetRegionData") {
        components::Region region = eyelib::serialize::SerializationTraits<components::Region>::Deserialize(
            record.payload.value("region", nlohmann::json{}), components::Region{}
        );
        return std::make_unique<SetRegionDataCommand>(DecodeEntity(record.payload["entity"]), region);
    }
    if (record.type == "AttachRegionRules") {
        return std::make_unique<AttachRegionRulesCommand>(
            DecodeEntity(record.payload["entity"]),
            eyelib::serialize::SerializationTraits<std::vector<std::string>>::Deserialize(
                record.payload.value("ruleIds", nlohmann::json::array()))
        );
    }
    if (record.type == "EnableSystemInRegion") {
        return std::make_unique<EnableSystemInRegionCommand>(
            DecodeEntity(record.payload["entity"]),
            record.payload.value("systemName", std::string{}),
            record.payload.value("enable", true)
        );
    }
    return nullptr;
}

} // namespace world
} // namespace eyelib
