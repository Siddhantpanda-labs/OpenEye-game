#pragma once

#include <cstdint>

namespace asset_pipe {

/** Canonical .mesh binary format. Shared by importer and runtime. 16-byte alignment. */
namespace MeshFormat {

constexpr uint32_t kMagic = 0x4D455348u;  // "MESH"
constexpr uint32_t kVersion = 1u;

struct MeshHeader {
    uint32_t magic = kMagic;
    uint32_t version = kVersion;

    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t submeshCount = 0;
    uint32_t streamCount = 0;

    uint32_t lodOffset = 0;
    uint32_t submeshOffset = 0;
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t boundsOffset = 0;
};

struct SubmeshRecord {
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
    uint32_t padding = 0;
};

struct MeshLOD {
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
    float screenSize = 0.f;
    uint32_t padding = 0;
};

struct BoundsRecord {
    float centerX = 0.f, centerY = 0.f, centerZ = 0.f;
    float extentsX = 0.f, extentsY = 0.f, extentsZ = 0.f;
    float radius = 0.f;
    uint32_t padding = 0;
};

/** Vertex layout in file (must match ImportVertex). */
struct VertexRecord {
    float px, py, pz;
    float nx, ny, nz;
    float tx, ty, tz, tw;
    float u0, v0;
    float u1, v1;
};

constexpr size_t kAlignment = 16u;

} // namespace MeshFormat
} // namespace asset_pipe
