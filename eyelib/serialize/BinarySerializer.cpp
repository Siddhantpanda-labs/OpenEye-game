#include "BinarySerializer.h"

#include "eyelib/core/Log.h"
#include "eyelib/scene/Scene.h"
#include "eyelib/world/EntityId.h"
#include "eyelib/world/World.h"
#include "eyelib/world/WorldView.h"
#include "eyelib/world/Commands/CommandTypes.h"
#include "eyelib/world/components/Camera.h"
#include "eyelib/world/components/Light.h"
#include "eyelib/world/components/AABB.h"
#include "eyelib/world/components/Renderable.h"
#include "eyelib/world/Serialization/WorldSerializationTraits.h"
#include "eyelib/serialize/SerializationTraits.h"

#include <json.hpp>

#if defined(EYELIB_WITH_LZ4) && EYELIB_WITH_LZ4
#include <lz4.h>
#endif
#if defined(EYELIB_WITH_ZSTD) && EYELIB_WITH_ZSTD
#include <zstd.h>
#endif

#include <algorithm>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace eyelib {
namespace serialize {

namespace {
using namespace eyelib::world::components;

static constexpr uint32_t kMagic = 0x45425932; // "EYB2"
static constexpr uint32_t kVersion = 3; // EntityId (index+generation), hierarchy+names+components (Transform,Camera,Light,AABB,Renderable)

static void WriteU32(std::vector<uint8_t>& out, uint32_t v) {
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + sizeof(v));
}

static void WriteU64(std::vector<uint8_t>& out, uint64_t v) {
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

static bool ReadU64(const std::vector<uint8_t>& data, size_t& offset, uint64_t& out) {
    if (offset + sizeof(uint64_t) > data.size()) return false;
    std::memcpy(&out, data.data() + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
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

static std::vector<uint8_t> RLECompress(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    size_t i = 0;
    while (i < input.size()) {
        uint8_t value = input[i];
        size_t run = 1;
        while (i + run < input.size() && input[i + run] == value && run < 255) {
            ++run;
        }
        output.push_back(static_cast<uint8_t>(run));
        output.push_back(value);
        i += run;
    }
    return output;
}

static bool RLEDecompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    output.clear();
    if (input.size() % 2 != 0) return false;
    for (size_t i = 0; i < input.size(); i += 2) {
        uint8_t count = input[i];
        uint8_t value = input[i + 1];
        output.insert(output.end(), count, value);
    }
    return true;
}

static std::vector<uint8_t> TryCompress(const std::vector<uint8_t>& input, BinaryCompression compression) {
    switch (compression) {
        case BinaryCompression::RLE: return RLECompress(input);
        case BinaryCompression::LZ4: {
#if defined(EYELIB_WITH_LZ4) && EYELIB_WITH_LZ4
            if (input.empty()) return {};
            const int srcSize = static_cast<int>(input.size());
            const int bound = LZ4_compressBound(srcSize);
            if (bound <= 0) return input;
            std::vector<uint8_t> out(static_cast<size_t>(bound));
            const int n = LZ4_compress_default(
                reinterpret_cast<const char*>(input.data()),
                reinterpret_cast<char*>(out.data()),
                srcSize, bound);
            if (n <= 0) return input;
            out.resize(static_cast<size_t>(n));
            return out;
#else
            core::Log::Warn("BinarySerializer: LZ4 not available, storing uncompressed.");
            return input;
#endif
        }
        case BinaryCompression::Zstd: {
#if defined(EYELIB_WITH_ZSTD) && EYELIB_WITH_ZSTD
            if (input.empty()) return {};
            const size_t srcSize = input.size();
            const size_t bound = ZSTD_compressBound(srcSize);
            if (ZSTD_isError(bound)) return input;
            std::vector<uint8_t> out(bound);
            const size_t n = ZSTD_compress(out.data(), bound, input.data(), srcSize, 3);
            if (ZSTD_isError(n)) return input;
            out.resize(n);
            return out;
#else
            core::Log::Warn("BinarySerializer: Zstd not available, storing uncompressed.");
            return input;
#endif
        }
        default: return input;
    }
}

static bool TryDecompress(const std::vector<uint8_t>& input, BinaryCompression compression,
                          size_t expectedOutSize, std::vector<uint8_t>& output) {
    switch (compression) {
        case BinaryCompression::RLE:
            return RLEDecompress(input, output);
        case BinaryCompression::LZ4: {
#if defined(EYELIB_WITH_LZ4) && EYELIB_WITH_LZ4
            if (input.empty() || expectedOutSize == 0) return false;
            output.resize(expectedOutSize);
            const int n = LZ4_decompress_safe(
                reinterpret_cast<const char*>(input.data()),
                reinterpret_cast<char*>(output.data()),
                static_cast<int>(input.size()),
                static_cast<int>(expectedOutSize));
            if (n < 0) return false;
            output.resize(static_cast<size_t>(n));
            return true;
#else
            output = input;
            return true;
#endif
        }
        case BinaryCompression::Zstd: {
#if defined(EYELIB_WITH_ZSTD) && EYELIB_WITH_ZSTD
            if (input.empty() || expectedOutSize == 0) return false;
            output.resize(expectedOutSize);
            const size_t n = ZSTD_decompress(output.data(), expectedOutSize, input.data(), input.size());
            if (ZSTD_isError(n)) return false;
            output.resize(n);
            return true;
#else
            output = input;
            return true;
#endif
        }
        default:
            output = input;
            return true;
    }
}

struct ComponentRecord {
    std::string name;
    uint32_t version = 0;
    std::vector<uint8_t> payload;
};

static void WriteEntityComponents(const scene::Scene& scene, eyelib::world::EntityId e,
    const eyelib::world::WorldView& view, std::vector<uint8_t>& payload) {
    std::vector<ComponentRecord> comps;

    if (auto opt = scene.GetTransform(e)) {
        nlohmann::json j = SerializationTraits<eyelib::math::Transform>::Serialize(*opt);
        std::string s = j.dump();
        comps.push_back({"Transform", 1, std::vector<uint8_t>(s.begin(), s.end())});
    }
    if (const auto* c = view.TryGetComponent<Camera>(e)) {
        nlohmann::json j = SerializationTraits<Camera>::Serialize(*c);
        comps.push_back({"Camera", 1, std::vector<uint8_t>(j.dump().begin(), j.dump().end())});
    }
    if (const auto* l = view.TryGetComponent<Light>(e)) {
        nlohmann::json j = SerializationTraits<Light>::Serialize(*l);
        comps.push_back({"Light", 1, std::vector<uint8_t>(j.dump().begin(), j.dump().end())});
    }
    if (const auto* a = view.TryGetComponent<AABB>(e)) {
        nlohmann::json j = SerializationTraits<AABB>::Serialize(*a);
        comps.push_back({"AABB", 1, std::vector<uint8_t>(j.dump().begin(), j.dump().end())});
    }
    if (const auto* r = view.TryGetComponent<Renderable>(e)) {
        nlohmann::json j = SerializationTraits<Renderable>::Serialize(*r);
        comps.push_back({"Renderable", 1, std::vector<uint8_t>(j.dump().begin(), j.dump().end())});
    }

    WriteU32(payload, static_cast<uint32_t>(comps.size()));
    for (const auto& comp : comps) {
        WriteString(payload, comp.name);
        WriteU32(payload, comp.version);
        WriteU32(payload, static_cast<uint32_t>(comp.payload.size()));
        if (!comp.payload.empty())
            payload.insert(payload.end(), comp.payload.begin(), comp.payload.end());
    }
}

static bool ApplyEntityComponents(scene::Scene& scene, scene::Entity entity,
    const std::vector<ComponentRecord>& comps) {
    eyelib::world::World& world = scene.World();
    for (const auto& comp : comps) {
        try {
            nlohmann::json j = nlohmann::json::parse(std::string(comp.payload.begin(), comp.payload.end()));
            if (comp.name == "Transform") {
                auto t = SerializationTraits<eyelib::math::Transform>::Deserialize(j, {});
                scene.SetTransform(entity, t);
            } else if (comp.name == "Camera") {
                auto v = SerializationTraits<Camera>::Deserialize(j, {});
                world.ApplyCommandSync(std::make_unique<eyelib::world::AddComponentCommand<Camera>>(entity, v));
            } else if (comp.name == "Light") {
                auto v = SerializationTraits<Light>::Deserialize(j, {});
                world.ApplyCommandSync(std::make_unique<eyelib::world::AddComponentCommand<Light>>(entity, v));
            } else if (comp.name == "AABB") {
                auto v = SerializationTraits<AABB>::Deserialize(j, {});
                world.ApplyCommandSync(std::make_unique<eyelib::world::AddComponentCommand<AABB>>(entity, v));
            } else if (comp.name == "Renderable") {
                auto v = SerializationTraits<Renderable>::Deserialize(j, {});
                world.ApplyCommandSync(std::make_unique<eyelib::world::AddComponentCommand<Renderable>>(entity, v));
            }
        } catch (...) {
            continue;
        }
    }
    return true;
}

struct EntityRecord {
    eyelib::world::EntityId id{};
    eyelib::world::EntityId parent{};
    std::string name;
    std::vector<ComponentRecord> components;
};

static uint64_t EntityIdKey(eyelib::world::EntityId e) {
    return (static_cast<uint64_t>(e.index) << 32) | e.generation;
}

static std::vector<uint8_t> BuildPayload(const scene::Scene& scene) {
    std::vector<uint8_t> payload;

    auto entities = scene.GetEntities();
    std::sort(entities.begin(), entities.end(),
        [](eyelib::world::EntityId a, eyelib::world::EntityId b) {
            if (a.index != b.index) return a.index < b.index;
            return a.generation < b.generation;
        });
    WriteU32(payload, static_cast<uint32_t>(entities.size()));

    eyelib::world::WorldView view = scene.World().GetView();
    for (auto e : entities) {
        eyelib::world::EntityId parent = scene.GetParent(e);
        WriteU32(payload, e.index);
        WriteU32(payload, e.generation);
        WriteU32(payload, parent.index);
        WriteU32(payload, parent.generation);
        WriteString(payload, scene.GetName(e).value_or(std::string{}));
        WriteEntityComponents(scene, e, view, payload);
    }

    return payload;
}

static bool ParsePayload(const std::vector<uint8_t>& payload, scene::Scene& scene) {
    size_t offset = 0;
    uint32_t entityCount = 0;
    if (!ReadU32(payload, offset, entityCount)) return false;

    std::vector<EntityRecord> records;
    records.reserve(entityCount);

    for (uint32_t i = 0; i < entityCount; ++i) {
        EntityRecord rec;
        if (!ReadU32(payload, offset, rec.id.index)) return false;
        if (!ReadU32(payload, offset, rec.id.generation)) return false;
        if (!ReadU32(payload, offset, rec.parent.index)) return false;
        if (!ReadU32(payload, offset, rec.parent.generation)) return false;
        if (!ReadString(payload, offset, rec.name)) return false;

        uint32_t componentCount = 0;
        if (!ReadU32(payload, offset, componentCount)) return false;
        for (uint32_t c = 0; c < componentCount; ++c) {
            ComponentRecord comp;
            if (!ReadString(payload, offset, comp.name)) return false;
            if (!ReadU32(payload, offset, comp.version)) return false;
            uint32_t payloadSize = 0;
            if (!ReadU32(payload, offset, payloadSize)) return false;
            if (offset + payloadSize > payload.size()) return false;
            comp.payload.assign(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                payload.begin() + static_cast<std::ptrdiff_t>(offset + payloadSize));
            offset += payloadSize;
            rec.components.push_back(std::move(comp));
        }
        records.push_back(std::move(rec));
    }

    scene.Clear();

    std::unordered_map<uint64_t, scene::Entity> idMap;
    idMap.reserve(records.size());
    for (const auto& rec : records) {
        scene::Entity newEntity = scene.CreateEntity(rec.name);
        idMap[EntityIdKey(rec.id)] = newEntity;
    }

    for (const auto& rec : records) {
        auto itChild = idMap.find(EntityIdKey(rec.id));
        if (itChild == idMap.end()) continue;
        scene::Entity newChild = itChild->second;
        if (rec.parent.index == eyelib::world::InvalidEntity.index && rec.parent.generation == eyelib::world::InvalidEntity.generation) {
            scene.SetParent(newChild, scene::NullEntity);
            continue;
        }
        auto itParent = idMap.find(EntityIdKey(rec.parent));
        if (itParent == idMap.end()) continue;
        scene.SetParent(newChild, itParent->second);
    }

    for (const auto& rec : records) {
        auto it = idMap.find(EntityIdKey(rec.id));
        if (it == idMap.end() || rec.components.empty()) continue;
        ApplyEntityComponents(scene, it->second, rec.components);
    }

    return true;
}
} // namespace

std::vector<uint8_t> BinarySerializer::SerializeScene(const scene::Scene& scene, BinaryCompression compression) {
    const auto rawPayload = BuildPayload(scene);
    const auto payload = TryCompress(rawPayload, compression);

    std::vector<uint8_t> out;
    WriteU32(out, kMagic);
    WriteU32(out, kVersion);
    WriteU32(out, static_cast<uint32_t>(compression));
    WriteU64(out, static_cast<uint64_t>(rawPayload.size()));
    WriteU64(out, static_cast<uint64_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

bool BinarySerializer::DeserializeScene(scene::Scene& scene, const std::vector<uint8_t>& data) {
    size_t offset = 0;
    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t compression = 0;
    uint64_t rawSize = 0;
    uint64_t payloadSize = 0;

    if (!ReadU32(data, offset, magic)) return false;
    if (!ReadU32(data, offset, version)) return false;
    if (!ReadU32(data, offset, compression)) return false;
    if (!ReadU64(data, offset, rawSize)) return false;
    if (!ReadU64(data, offset, payloadSize)) return false;

    if (magic != kMagic || version != kVersion) return false;
    if (offset + payloadSize > data.size()) return false;

    std::vector<uint8_t> payload(data.begin() + offset, data.begin() + offset + payloadSize);
    BinaryCompression mode = static_cast<BinaryCompression>(compression);
    std::vector<uint8_t> decoded;
    const size_t expectedSize = (rawSize != 0) ? static_cast<size_t>(rawSize) : payload.size();
    if (!TryDecompress(payload, mode, expectedSize, decoded)) return false;
    if (decoded.empty()) return false;
    payload = std::move(decoded);

    if (rawSize != 0 && payload.size() != static_cast<size_t>(rawSize)) return false;
    return ParsePayload(payload, scene);
}

bool BinarySerializer::SaveToFile(const scene::Scene& scene, const std::string& path, BinaryCompression compression) {
    const auto data = SerializeScene(scene, compression);
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return out.good();
}

bool BinarySerializer::LoadFromFile(scene::Scene& scene, const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return DeserializeScene(scene, data);
}

} // namespace serialize
} // namespace eyelib
