#include "eyelib/geometry/MeshBinaryFormat.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"

#include <cstring>
#include <vector>

namespace eyelib {
namespace geometry {
namespace MeshBin {

namespace {

using Vertex = render::Vertex;
using Submesh = render::Submesh;

void WriteU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 24));
}

void WriteChunkHeader(std::vector<uint8_t>& out, ChunkId id, uint32_t payloadSize) {
    WriteU32(out, static_cast<uint32_t>(id));
    WriteU32(out, payloadSize);
}

void ReadU32(const uint8_t*& p, const uint8_t* end, uint32_t& out) {
    if (p + 4 > end) { out = 0; return; }
    out = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
          (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    p += 4;
}

bool ReadChunkHeader(const uint8_t*& p, const uint8_t* end, uint32_t& chunkId, uint32_t& payloadSize) {
    if (p + 8 > end) return false;
    ReadU32(p, end, chunkId);
    ReadU32(p, end, payloadSize);
    return true;
}

} // namespace

bool WriteToFile(const EditableMesh& mesh, const std::string& path) {
    std::vector<uint8_t> data = Serialize(mesh);
    if (data.empty()) return false;
    return platform::filesystem::WriteFile(path, data.data(), data.size());
}

bool ReadFromFile(const std::string& path, EditableMesh& out) {
    std::vector<uint8_t> data = platform::filesystem::ReadFile(path);
    if (data.empty()) return false;
    return Deserialize(data.data(), data.size(), out);
}

std::vector<uint8_t> Serialize(const EditableMesh& mesh) {
    std::vector<uint8_t> out;
    out.reserve(1024 * 1024);

    WriteU32(out, kMagic);
    WriteU32(out, kVersion);

    const std::vector<Vertex>& verts = mesh.GetVertices();
    const std::vector<uint32_t>& inds = mesh.GetIndices();
    const std::vector<Submesh>& subs = mesh.GetSubmeshes();

    // VERT
    uint32_t nv = static_cast<uint32_t>(verts.size());
    uint32_t vertPayload = 4 + nv * static_cast<uint32_t>(sizeof(Vertex));
    WriteChunkHeader(out, VERT, vertPayload);
    WriteU32(out, nv);
    for (const Vertex& v : verts) {
        out.insert(out.end(), reinterpret_cast<const uint8_t*>(&v), reinterpret_cast<const uint8_t*>(&v) + sizeof(Vertex));
    }

    // INDX
    uint32_t ni = static_cast<uint32_t>(inds.size());
    uint32_t indPayload = 4 + ni * 4u;
    WriteChunkHeader(out, INDX, indPayload);
    WriteU32(out, ni);
    for (uint32_t i : inds) WriteU32(out, i);

    // SUBM
    uint32_t ns = static_cast<uint32_t>(subs.size());
    uint32_t subPayload = 4 + ns * (4u + 4u + 4u);
    WriteChunkHeader(out, SUBM, subPayload);
    WriteU32(out, ns);
    for (const Submesh& s : subs) {
        WriteU32(out, s.indexOffset);
        WriteU32(out, s.indexCount);
        WriteU32(out, static_cast<uint32_t>(static_cast<int32_t>(s.materialIndex)));
    }

    // SKIN (skeleton ID: 16 bytes)
    if (mesh.HasSkeleton()) {
        WriteChunkHeader(out, SKIN, 16);
        uint64_t hi = mesh.GetSkeletonId().high;
        uint64_t lo = mesh.GetSkeletonId().low;
        for (int i = 0; i < 8; ++i) { out.push_back(static_cast<uint8_t>(hi)); hi >>= 8; }
        for (int i = 0; i < 8; ++i) { out.push_back(static_cast<uint8_t>(lo)); lo >>= 8; }
    }

    return out;
}

bool Deserialize(const uint8_t* data, size_t size, EditableMesh& out) {
    if (!data || size < 8) return false;
    const uint8_t* p = data;
    const uint8_t* end = data + size;

    uint32_t magic = 0, version = 0;
    ReadU32(p, end, magic);
    ReadU32(p, end, version);
    if (magic != kMagic) {
        core::Log::WarnF("MeshBinaryFormat: bad magic 0x%08X", magic);
        return false;
    }
    if (version != kVersion) {
        core::Log::WarnF("MeshBinaryFormat: unsupported version %u", version);
        return false;
    }

    out.SetVertices({});
    out.SetIndices({});
    out.GetSubmeshes().clear();
    out.SetSkeletonId(asset::Guid::Invalid());

    while (p + 8 <= end) {
        uint32_t chunkId = 0, payloadSize = 0;
        if (!ReadChunkHeader(p, end, chunkId, payloadSize)) break;
        const uint8_t* chunkEnd = p + payloadSize;
        if (chunkEnd > end) break;

        switch (chunkId) {
            case VERT: {
                uint32_t nv = 0;
                ReadU32(p, end, nv);
                std::vector<Vertex> verts(nv);
                size_t need = nv * sizeof(Vertex);
                if (p + need > chunkEnd) break;
                std::memcpy(verts.data(), p, need);
                p += need;
                out.SetVertices(std::move(verts));
                break;
            }
            case INDX: {
                uint32_t ni = 0;
                ReadU32(p, end, ni);
                std::vector<uint32_t> inds(ni);
                for (uint32_t i = 0; i < ni && p + 4 <= chunkEnd; ++i) {
                    ReadU32(p, end, inds[i]);
                }
                p = chunkEnd;
                out.SetIndices(std::move(inds));
                break;
            }
            case SUBM: {
                uint32_t ns = 0;
                ReadU32(p, end, ns);
                out.GetSubmeshes().clear();
                for (uint32_t i = 0; i < ns && p + 12 <= chunkEnd; ++i) {
                    uint32_t off = 0, cnt = 0, matu = 0;
                    ReadU32(p, end, off);
                    ReadU32(p, end, cnt);
                    ReadU32(p, end, matu);
                    out.AddSubmesh(off, cnt, static_cast<int>(static_cast<int32_t>(matu)));
                }
                p = chunkEnd;
                break;
            }
            case SKIN: {
                if (payloadSize >= 16) {
                    uint64_t hi = 0, lo = 0;
                    for (int i = 0; i < 8 && p < chunkEnd; ++i) { hi |= static_cast<uint64_t>(*p++) << (i * 8); }
                    for (int i = 0; i < 8 && p < chunkEnd; ++i) { lo |= static_cast<uint64_t>(*p++) << (i * 8); }
                    asset::Guid g;
                    g.high = hi;
                    g.low = lo;
                    out.SetSkeletonId(g);
                }
                p = chunkEnd;
                break;
            }
            default:
                p = chunkEnd;
                break;
        }
    }

    return true;
}

} // namespace MeshBin
} // namespace geometry
} // namespace eyelib
