#include "asset_pipe/writers/mesh_writer.h"
#include "asset_pipe/core/mesh_format.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

namespace asset_pipe {

namespace {

using namespace MeshFormat;

size_t AlignUp(size_t value, size_t align) {
    return (value + align - 1) & ~(align - 1);
}

constexpr size_t kAlign = kAlignment;

bool WriteBytes(std::ostream& out, const void* data, size_t size) {
    return out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size)).good();
}

bool WriteU32(std::ostream& out, uint32_t v) {
    return WriteBytes(out, &v, sizeof(v));
}

bool WriteFloat(std::ostream& out, float v) {
    return WriteBytes(out, &v, sizeof(v));
}

} // namespace

bool MeshWriter::Write(const ImportMesh& mesh, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    const uint32_t vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    const uint32_t indexCount = static_cast<uint32_t>(mesh.indices.size());
    const uint32_t submeshCount = static_cast<uint32_t>(mesh.submeshes.size());

    // Offsets (we'll fix header after we know layout)
    size_t offset = sizeof(MeshHeader);

    // LOD table: single LOD for now
    const uint32_t lodCount = 1u;
    size_t lodTableSize = lodCount * sizeof(MeshLOD);
    lodTableSize = AlignUp(lodTableSize, kAlign);
    uint32_t lodOffset = static_cast<uint32_t>(offset);
    offset += lodTableSize;

    // Submesh table
    size_t submeshTableSize = submeshCount * sizeof(SubmeshRecord);
    submeshTableSize = AlignUp(submeshTableSize, kAlign);
    uint32_t submeshOffset = static_cast<uint32_t>(offset);
    offset += submeshTableSize;

    // Vertex data
    size_t vertexDataSize = vertexCount * sizeof(VertexRecord);
    vertexDataSize = AlignUp(vertexDataSize, kAlign);
    uint32_t vertexOffset = static_cast<uint32_t>(offset);
    offset += vertexDataSize;

    // Index data
    size_t indexDataSize = indexCount * sizeof(uint32_t);
    indexDataSize = AlignUp(indexDataSize, kAlign);
    uint32_t indexOffset = static_cast<uint32_t>(offset);
    offset += indexDataSize;

    // Bounds
    uint32_t boundsOffset = static_cast<uint32_t>(offset);

    MeshHeader header;
    header.vertexCount = vertexCount;
    header.indexCount = indexCount;
    header.submeshCount = submeshCount;
    header.streamCount = 1u;
    header.lodOffset = lodOffset;
    header.submeshOffset = submeshOffset;
    header.vertexOffset = vertexOffset;
    header.indexOffset = indexOffset;
    header.boundsOffset = boundsOffset;

    if (!WriteBytes(file, &header, sizeof(header))) return false;

    // LOD table
    MeshLOD lod;
    lod.indexStart = 0;
    lod.indexCount = indexCount;
    lod.screenSize = 1.f;
    if (!WriteBytes(file, &lod, sizeof(lod))) return false;
    for (size_t pad = sizeof(lod); pad < lodTableSize; pad += sizeof(uint32_t))
        if (!WriteU32(file, 0)) return false;

    // Submesh table
    for (const auto& sub : mesh.submeshes) {
        SubmeshRecord rec;
        rec.indexStart = sub.indexStart;
        rec.indexCount = sub.indexCount;
        rec.materialIndex = sub.materialIndex;
        if (!WriteBytes(file, &rec, sizeof(rec))) return false;
    }
    for (size_t pad = submeshCount * sizeof(SubmeshRecord); pad < submeshTableSize; pad += sizeof(uint32_t))
        if (!WriteU32(file, 0)) return false;

    // Vertices
    for (const auto& v : mesh.vertices) {
        VertexRecord rec;
        rec.px = v.position.x; rec.py = v.position.y; rec.pz = v.position.z;
        rec.nx = v.normal.x; rec.ny = v.normal.y; rec.nz = v.normal.z;
        rec.tx = v.tangent.x; rec.ty = v.tangent.y; rec.tz = v.tangent.z; rec.tw = v.tangent.w;
        rec.u0 = v.uv0.x; rec.v0 = v.uv0.y;
        rec.u1 = v.uv1.x; rec.v1 = v.uv1.y;
        if (!WriteBytes(file, &rec, sizeof(rec))) return false;
    }
    for (size_t pad = vertexCount * sizeof(VertexRecord); pad < vertexDataSize; pad += sizeof(uint32_t))
        if (!WriteU32(file, 0)) return false;

    // Indices
    if (!WriteBytes(file, mesh.indices.data(), indexCount * sizeof(uint32_t))) return false;
    for (size_t pad = indexDataSize - indexCount * sizeof(uint32_t); pad > 0; pad -= sizeof(uint32_t))
        if (!WriteU32(file, 0)) return false;

    // Bounds
    BoundsRecord bounds;
    bounds.centerX = (mesh.bounds.min.x + mesh.bounds.max.x) * 0.5f;
    bounds.centerY = (mesh.bounds.min.y + mesh.bounds.max.y) * 0.5f;
    bounds.centerZ = (mesh.bounds.min.z + mesh.bounds.max.z) * 0.5f;
    bounds.extentsX = (mesh.bounds.max.x - mesh.bounds.min.x) * 0.5f;
    bounds.extentsY = (mesh.bounds.max.y - mesh.bounds.min.y) * 0.5f;
    bounds.extentsZ = (mesh.bounds.max.z - mesh.bounds.min.z) * 0.5f;
    float dx = bounds.extentsX, dy = bounds.extentsY, dz = bounds.extentsZ;
    bounds.radius = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (!WriteBytes(file, &bounds, sizeof(bounds))) return false;

    return true;
}

} // namespace asset_pipe
