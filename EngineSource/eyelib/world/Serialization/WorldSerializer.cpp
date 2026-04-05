#include "WorldSerializer.h"

#include "eyelib/world/SkyboxAssetId.h"

#include "ComponentSerializer.h"
#include "eyelib/world/EntityRegistry.h"
#include "eyelib/world/WorldGraph/Hierarchy.h"
#include "eyelib/world/WorldGraph/Metadata.h"
#include "eyelib/world/WorldGraph/Tags.h"

#include "eyelib/serialize/SerializationTraits.h"

#include <json.hpp>

#include <fstream>

namespace eyelib {
namespace world {

namespace {
static std::string NormalizeRenderPipelineMode(const std::string& mode) {
    if (mode == "Hybrid" || mode == "Forward" || mode == "ForwardPlus")
        return mode;
    if (mode == "Deferred")
        return "Hybrid";
    return "Hybrid";
}

static nlohmann::json SerializeEntityId(const EntityId& id) {
    nlohmann::json j;
    j["index"] = id.index;
    j["generation"] = id.generation;
    return j;
}

static EntityId DeserializeEntityId(const nlohmann::json& j) {
    EntityId id;
    id.index = j.value("index", 0u);
    id.generation = j.value("generation", 0u);
    return id;
}
} // namespace

bool WorldSerializer::Save(const WorldState& state, const std::string& path) const {
    nlohmann::json root = Serialize(state);
    if (root.is_null()) {
        return false;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }
    out << root.dump(2);
    return true;
}

nlohmann::json WorldSerializer::Serialize(const WorldState& state) const {
    nlohmann::json root;
    root["version"] = kVersion;

    nlohmann::json registry;
    registry["generations"] = state.Entities().Generations();
    registry["freeList"] = state.Entities().FreeList();
    root["registry"] = std::move(registry);

    nlohmann::json graph;
    nlohmann::json metadata = nlohmann::json::array();
    for (const auto& [entity, entry] : state.GraphMetadata().All()) {
        nlohmann::json item;
        item["entity"] = SerializeEntityId(entity);
        item["name"] = entry.name;
        item["flags"] = entry.flags;
        metadata.push_back(std::move(item));
    }
    graph["metadata"] = std::move(metadata);

    nlohmann::json hierarchy = nlohmann::json::array();
    for (const auto& [child, parent] : state.GraphHierarchy().Parents()) {
        nlohmann::json item;
        item["child"] = SerializeEntityId(child);
        item["parent"] = SerializeEntityId(parent);
        hierarchy.push_back(std::move(item));
    }
    graph["hierarchy"] = std::move(hierarchy);

    nlohmann::json tags = nlohmann::json::array();
    for (const auto& [entity, tagSet] : state.GraphTags().All()) {
        nlohmann::json item;
        item["entity"] = SerializeEntityId(entity);
        std::vector<std::string> values(tagSet.begin(), tagSet.end());
        item["tags"] = eyelib::serialize::SerializationTraits<std::vector<std::string>>::Serialize(values);
        tags.push_back(std::move(item));
    }
    graph["tags"] = std::move(tags);
    root["graph"] = std::move(graph);

    nlohmann::json components;
    auto& serializer = ComponentSerializer::Get();
    for (const auto& [typeKey, store] : state.ComponentStores()) {
        const auto* desc = serializer.GetDescriptor(typeKey);
        if (!desc || !desc->serializeStore) {
            continue;
        }
        nlohmann::json block;
        block["version"] = desc->version;
        block["items"] = desc->serializeStore(*store);
        components[desc->name] = std::move(block);
    }
    root["components"] = std::move(components);

    root["skybox"] = state.GetSkyboxAssetId();
    root["renderPipelineMode"] = NormalizeRenderPipelineMode(state.GetRenderPipelineMode());

    return root;
}

bool WorldSerializer::Load(WorldState& state, const std::string& path) const {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    nlohmann::json root;
    try {
        in >> root;
    } catch (...) {
        return false;
    }
    return Deserialize(state, root);
}

bool WorldSerializer::Deserialize(WorldState& state, const nlohmann::json& root) const {
    if (!root.contains("version") || root["version"].get<uint32_t>() != kVersion) {
        return false;
    }

    state.Clear();

    if (root.contains("registry")) {
        const auto& reg = root["registry"];
        std::vector<uint32_t> generations = reg.value("generations", std::vector<uint32_t>{});
        std::vector<uint32_t> freeList = reg.value("freeList", std::vector<uint32_t>{});
        state.Entities().Restore(generations, freeList);
    }

    if (root.contains("graph")) {
        const auto& graph = root["graph"];
        if (graph.contains("metadata")) {
            for (const auto& item : graph["metadata"]) {
                EntityId id = DeserializeEntityId(item["entity"]);
                MetadataEntry entry;
                entry.name = item.value("name", std::string{});
                entry.flags = item.value("flags", 0u);
                state.GraphMetadata().Set(id, entry);
            }
        }
        if (graph.contains("hierarchy")) {
            for (const auto& item : graph["hierarchy"]) {
                EntityId child = DeserializeEntityId(item["child"]);
                EntityId parent = DeserializeEntityId(item["parent"]);
                state.GraphHierarchy().SetParent(child, parent);
            }
        }
        if (graph.contains("tags")) {
            for (const auto& item : graph["tags"]) {
                EntityId id = DeserializeEntityId(item["entity"]);
                std::vector<std::string> values = eyelib::serialize::SerializationTraits<std::vector<std::string>>::Deserialize(item.value("tags", nlohmann::json::array()));
                for (const auto& tag : values) {
                    state.GraphTags().Add(id, tag);
                }
            }
        }
    }

    if (root.contains("components")) {
        const auto& comps = root["components"];
        auto& serializer = ComponentSerializer::Get();
        for (auto it = comps.begin(); it != comps.end(); ++it) {
            const std::string name = it.key();
            const auto* desc = serializer.GetDescriptorByName(name);
            if (!desc || !desc->deserializeStore || !desc->createStore) {
                continue;
            }
            if (!state.FindComponentStore(desc->typeKey)) {
                state.RegisterComponentStore(desc->typeKey, desc->createStore());
            }
            auto* store = state.FindComponentStore(desc->typeKey);
            if (!store) {
                continue;
            }
            const auto& block = it.value();
            if (!block.contains("items")) {
                continue;
            }
            desc->deserializeStore(*store, block["items"]);
        }
    }

    if (root.contains("skybox") && root["skybox"].is_string()) {
        state.SetSkyboxAssetId(root["skybox"].get<std::string>());
    } else {
        state.SetSkyboxAssetId(skybox::kProceduralId);
    }
    if (root.contains("renderPipelineMode") && root["renderPipelineMode"].is_string()) {
        state.SetRenderPipelineMode(NormalizeRenderPipelineMode(root["renderPipelineMode"].get<std::string>()));
    } else {
        state.SetRenderPipelineMode("Hybrid");
    }

    return true;
}

} // namespace world
} // namespace eyelib
