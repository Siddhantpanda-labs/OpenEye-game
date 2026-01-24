#include "WorldSnapshot.h"

#include "eyelib/serialize/ComponentSerializer.h"
#include "eyelib/core/Log.h"
#include "eyelib/ecs/components/Transform.h"

#include <json.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <unordered_map>
#include "entt/entt.hpp"

namespace eyelib {
namespace serialize {

namespace {
static constexpr uint32_t kSnapshotVersion = 2;

static void WriteU32(std::vector<uint8_t>& out, uint32_t v) {
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + sizeof(v));
}

static void WriteBytes(std::vector<uint8_t>& out, const void* data, size_t size) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(data);
    out.insert(out.end(), bytes, bytes + size);
}

static void WriteString(std::vector<uint8_t>& out, const std::string& value) {
    WriteU32(out, static_cast<uint32_t>(value.size()));
    if (!value.empty()) WriteBytes(out, value.data(), value.size());
}

static bool ReadU32(const std::vector<uint8_t>& data, size_t& offset, uint32_t& out) {
    if (offset + sizeof(uint32_t) > data.size()) return false;
    std::memcpy(&out, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    return true;
}

static bool ReadString(const std::vector<uint8_t>& data, size_t& offset, std::string& out) {
    uint32_t length = 0;
    if (!ReadU32(data, offset, length)) return false;
    if (offset + length > data.size()) return false;
    out.assign(reinterpret_cast<const char*>(data.data() + offset), length);
    offset += length;
    return true;
}
} // namespace

Snapshot WorldSnapshot::Capture(const ecs::World& world) {
    Snapshot snapshot;
    snapshot.timestamp = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    const auto& registry = world.Components();
    const auto& storage = world.Storage().Registry();
    auto& serializer = ComponentSerializer::Get();

    std::vector<entt::entity> entities;
    auto view = storage.view<ecs::components::Transform>();
    for (auto entity : view) {
        entities.push_back(entity);
    }
    std::sort(entities.begin(), entities.end(), [](entt::entity a, entt::entity b) {
        return static_cast<entt::id_type>(a) < static_cast<entt::id_type>(b);
    });

    WriteU32(snapshot.data, kSnapshotVersion);
    WriteU32(snapshot.data, static_cast<uint32_t>(entities.size()));

    for (auto entity : entities) {
        WriteU32(snapshot.data, static_cast<uint32_t>(static_cast<entt::id_type>(entity)));
        uint32_t parent = 0u;
        if (const auto* tr = storage.try_get<ecs::components::Transform>(entity)) {
            parent = tr->parent;
        }
        WriteU32(snapshot.data, parent);

        struct CompRec { std::string name; uint32_t version; std::vector<uint8_t> payload; };
        std::vector<CompRec> comps;

        for (auto type : registry.GetComponentTypes()) {
            const auto* ops = registry.GetOps(type);
            const auto* desc = serializer.GetDescriptor(type);
            if (!ops || !ops->getConst || !desc) continue;
            const void* ptr = ops->getConst(storage, entity);
            if (!ptr) continue;
            nlohmann::json json = serializer.SerializeComponent(type, ptr);
            std::string text = json.dump();
            comps.push_back(CompRec{ desc->name, desc->version, std::vector<uint8_t>(text.begin(), text.end()) });
        }

        std::sort(comps.begin(), comps.end(), [](const CompRec& a, const CompRec& b) {
            return a.name < b.name;
        });

        WriteU32(snapshot.data, static_cast<uint32_t>(comps.size()));
        for (const auto& comp : comps) {
            WriteString(snapshot.data, comp.name);
            WriteU32(snapshot.data, comp.version);
            WriteU32(snapshot.data, static_cast<uint32_t>(comp.payload.size()));
            if (!comp.payload.empty()) {
                WriteBytes(snapshot.data, comp.payload.data(), comp.payload.size());
            }
        }
    }

    return snapshot;
}

bool WorldSnapshot::Restore(ecs::World& world, const Snapshot& snapshot) {
    size_t offset = 0;
    uint32_t version = 0;
    uint32_t entityCount = 0;
    if (!ReadU32(snapshot.data, offset, version)) return false;
    if (version != kSnapshotVersion) return false;
    if (!ReadU32(snapshot.data, offset, entityCount)) return false;

    struct Record {
        uint32_t id = 0;
        uint32_t parent = 0;
        std::vector<std::pair<std::string, std::pair<uint32_t, std::vector<uint8_t>>>> components;
    };

    std::vector<Record> records;
    records.reserve(entityCount);

    for (uint32_t i = 0; i < entityCount; ++i) {
        Record rec;
        if (!ReadU32(snapshot.data, offset, rec.id)) return false;
        if (!ReadU32(snapshot.data, offset, rec.parent)) return false;
        uint32_t componentCount = 0;
        if (!ReadU32(snapshot.data, offset, componentCount)) return false;
        rec.components.reserve(componentCount);

        for (uint32_t c = 0; c < componentCount; ++c) {
            std::string name;
            if (!ReadString(snapshot.data, offset, name)) return false;
            uint32_t compVersion = 0;
            if (!ReadU32(snapshot.data, offset, compVersion)) return false;
            uint32_t payloadSize = 0;
            if (!ReadU32(snapshot.data, offset, payloadSize)) return false;
            if (offset + payloadSize > snapshot.data.size()) return false;
            std::vector<uint8_t> payload(snapshot.data.begin() + offset, snapshot.data.begin() + offset + payloadSize);
            offset += payloadSize;
            rec.components.emplace_back(name, std::make_pair(compVersion, std::move(payload)));
        }
        records.push_back(std::move(rec));
    }

    world.Registry().clear();

    std::unordered_map<uint32_t, uint32_t> idMap;
    idMap.reserve(records.size());
    for (const auto& rec : records) {
        const auto newId = world.CreateEntity();
        idMap[rec.id] = newId;
    }

    auto& registry = world.Components();
    auto& storage = world.Storage().Registry();
    auto& serializer = ComponentSerializer::Get();

    for (const auto& rec : records) {
        auto it = idMap.find(rec.id);
        if (it == idMap.end()) continue;
        const uint32_t newEntity = it->second;

        for (const auto& entry : rec.components) {
            const auto typeId = serializer.FindTypeId(entry.first);
            if (typeId == ecs::ComponentTypeId{}) continue;
            const auto* desc = serializer.GetDescriptor(typeId);
            if (!desc || entry.second.first > desc->version) {
                core::Log::WarnF("WorldSnapshot: skipping component '%s' (version %u)", entry.first.c_str(), entry.second.first);
                continue;
            }

            nlohmann::json json;
            try {
                json = nlohmann::json::parse(std::string(entry.second.second.begin(), entry.second.second.end()));
            } catch (const std::exception& e) {
                core::Log::WarnF("WorldSnapshot: parse error '%s': %s", entry.first.c_str(), e.what());
                continue;
            }

            void* comp = serializer.DeserializeComponent(typeId, json);
            const auto* ops = registry.GetOps(typeId);
            if (comp && ops && ops->add && ops->destroy) {
                ops->add(storage, ecs::detail::ToEnTT(newEntity), comp);
                ops->destroy(comp);
            }
        }
    }

    for (const auto& rec : records) {
        auto itChild = idMap.find(rec.id);
        if (itChild == idMap.end()) continue;
        const uint32_t newEntity = itChild->second;
        auto* tr = storage.try_get<ecs::components::Transform>(ecs::detail::ToEnTT(newEntity));
        if (!tr) continue;
        if (rec.parent == 0u) {
            tr->parent = ecs::NullEntity;
        } else {
            auto itParent = idMap.find(rec.parent);
            tr->parent = itParent != idMap.end() ? itParent->second : ecs::NullEntity;
        }
        tr->dirty = true;
    }
    return true;
}

} // namespace serialize
} // namespace eyelib
