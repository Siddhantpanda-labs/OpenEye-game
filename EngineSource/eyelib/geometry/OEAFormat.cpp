#include "eyelib/geometry/EditableMesh.h"
#include "eyelib/geometry/OEAFormat.h"
#include "eyelib/geometry/MeshBinaryFormat.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"

#include <cstring>

namespace eyelib {
namespace geometry {
namespace OEA {

namespace {

void WriteU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 24));
}

void ReadU32(const uint8_t*& p, const uint8_t* end, uint32_t& out) {
    if (p + 4 > end) { out = 0; return; }
    out = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
          (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    p += 4;
}

} // namespace

bool WriteToFile(AssetType type, const uint8_t* payload, size_t payloadSize, const std::string& path) {
    std::vector<uint8_t> out;
    out.reserve(kHeaderSize + payloadSize);
    WriteU32(out, kMagic);
    WriteU32(out, kVersion);
    WriteU32(out, static_cast<uint32_t>(type));
    WriteU32(out, 0u); // reserved
    if (payload && payloadSize > 0)
        out.insert(out.end(), payload, payload + payloadSize);
    return platform::filesystem::WriteFile(path, out.data(), out.size());
}

bool ReadFromFile(const std::string& path, AssetType& outType, std::vector<uint8_t>& outPayload) {
    outType = AssetType::Unknown;
    outPayload.clear();
    std::vector<uint8_t> data = platform::filesystem::ReadFile(path);
    if (data.size() < kHeaderSize) return false;
    const uint8_t* p = data.data();
    const uint8_t* end = data.data() + data.size();
    uint32_t magic = 0, version = 0, type = 0;
    ReadU32(p, end, magic);
    ReadU32(p, end, version);
    ReadU32(p, end, type);
    if (magic != kMagic) {
        core::Log::WarnF("OEA: bad magic 0x%08X", magic);
        return false;
    }
    if (version != kVersion) {
        core::Log::WarnF("OEA: unsupported version %u", version);
        return false;
    }
    outType = static_cast<AssetType>(type);
    size_t payloadLen = data.size() - kHeaderSize;
    if (payloadLen > 0)
        outPayload.assign(p, p + payloadLen);
    return true;
}

bool IsOEAFile(const uint8_t* data, size_t size) {
    if (!data || size < 4) return false;
    uint32_t magic = static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
                     (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
    return magic == kMagic;
}

bool WriteMeshToFile(const EditableMesh& mesh, const std::string& path) {
    std::vector<uint8_t> payload = MeshBin::Serialize(mesh);
    if (payload.empty()) return false;
    return WriteToFile(AssetType::Mesh, payload.data(), payload.size(), path);
}

bool ReadMeshFromFile(const std::string& path, EditableMesh& out) {
    AssetType type = AssetType::Unknown;
    std::vector<uint8_t> payload;
    if (!ReadFromFile(path, type, payload)) return false;
    if (type != AssetType::Mesh) return false;
    return MeshBin::Deserialize(payload.data(), payload.size(), out);
}

} // namespace OEA
} // namespace geometry
} // namespace eyelib
