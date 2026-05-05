#include "asset/readers/mesh_reader.h"
#include "asset/core/mesh_format.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

namespace asset {

namespace {

using namespace MeshFormat;

constexpr size_t kAlign = kAlignment;

size_t AlignUp(size_t value, size_t align) {
    return (value + align - 1) & ~(align - 1);
}

bool ReadBytes(std::istream& in, void* data, size_t size) {
    return in.read(static_cast<char*>(data), static_cast<std::streamsize>(size)).good();
}

bool ParseMeshBinary(const uint8_t* data, size_t size, ImportMesh& out) {
    if (!data || size < sizeof(MeshHeader)) return false;
    const auto* header = reinterpret_cast<const MeshHeader*>(data);
    if (header->magic != kMagic || header->version != kVersion) return false;

    auto fits = [size](uint32_t offset, size_t bytes) -> bool {
        return static_cast<size_t>(offset) <= size && bytes <= (size - static_cast<size_t>(offset));
    };
    if (!fits(header->submeshOffset, static_cast<size_t>(header->submeshCount) * sizeof(SubmeshRecord))) return false;
    if (!fits(header->vertexOffset, static_cast<size_t>(header->vertexCount) * sizeof(VertexRecord))) return false;
    if (!fits(header->indexOffset, static_cast<size_t>(header->indexCount) * sizeof(uint32_t))) return false;
    if (header->boundsOffset && !fits(header->boundsOffset, sizeof(BoundsRecord))) return false;

    out = ImportMesh{};
    out.vertices.resize(header->vertexCount);
    out.indices.resize(header->indexCount);
    out.submeshes.resize(header->submeshCount);

    const auto* submeshes = reinterpret_cast<const SubmeshRecord*>(data + header->submeshOffset);
    for (uint32_t i = 0; i < header->submeshCount; ++i) {
        out.submeshes[i].indexStart = submeshes[i].indexStart;
        out.submeshes[i].indexCount = submeshes[i].indexCount;
        out.submeshes[i].materialIndex = submeshes[i].materialIndex;
    }

    const auto* verts = reinterpret_cast<const VertexRecord*>(data + header->vertexOffset);
    for (uint32_t i = 0; i < header->vertexCount; ++i) {
        out.vertices[i].position.x = verts[i].px; out.vertices[i].position.y = verts[i].py; out.vertices[i].position.z = verts[i].pz;
        out.vertices[i].normal.x = verts[i].nx; out.vertices[i].normal.y = verts[i].ny; out.vertices[i].normal.z = verts[i].nz;
        out.vertices[i].tangent.x = verts[i].tx; out.vertices[i].tangent.y = verts[i].ty; out.vertices[i].tangent.z = verts[i].tz; out.vertices[i].tangent.w = verts[i].tw;
        out.vertices[i].uv0.x = verts[i].u0; out.vertices[i].uv0.y = verts[i].v0;
        out.vertices[i].uv1.x = verts[i].u1; out.vertices[i].uv1.y = verts[i].v1;
    }

    const auto* idx = reinterpret_cast<const uint32_t*>(data + header->indexOffset);
    for (uint32_t i = 0; i < header->indexCount; ++i) out.indices[i] = idx[i];

    if (header->boundsOffset) {
        const auto* bounds = reinterpret_cast<const BoundsRecord*>(data + header->boundsOffset);
        out.bounds.min.x = bounds->centerX - bounds->extentsX;
        out.bounds.min.y = bounds->centerY - bounds->extentsY;
        out.bounds.min.z = bounds->centerZ - bounds->extentsZ;
        out.bounds.max.x = bounds->centerX + bounds->extentsX;
        out.bounds.max.y = bounds->centerY + bounds->extentsY;
        out.bounds.max.z = bounds->centerZ + bounds->extentsZ;
    }
    return true;
}

} // namespace

bool MeshReader::Read(const std::string& path, ImportMesh& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    MeshHeader header;
    if (!ReadBytes(file, &header, sizeof(header))) return false;
    if (header.magic != kMagic || header.version != kVersion) return false;

    out = ImportMesh{};
    out.vertices.resize(header.vertexCount);
    out.indices.resize(header.indexCount);
    out.submeshes.resize(header.submeshCount);

    file.seekg(header.submeshOffset);
    for (uint32_t i = 0; i < header.submeshCount; ++i) {
        SubmeshRecord rec;
        if (!ReadBytes(file, &rec, sizeof(rec))) return false;
        out.submeshes[i].indexStart = rec.indexStart;
        out.submeshes[i].indexCount = rec.indexCount;
        out.submeshes[i].materialIndex = rec.materialIndex;
    }

    file.seekg(header.vertexOffset);
    for (uint32_t i = 0; i < header.vertexCount; ++i) {
        VertexRecord rec;
        if (!ReadBytes(file, &rec, sizeof(rec))) return false;
        out.vertices[i].position.x = rec.px; out.vertices[i].position.y = rec.py; out.vertices[i].position.z = rec.pz;
        out.vertices[i].normal.x = rec.nx; out.vertices[i].normal.y = rec.ny; out.vertices[i].normal.z = rec.nz;
        out.vertices[i].tangent.x = rec.tx; out.vertices[i].tangent.y = rec.ty; out.vertices[i].tangent.z = rec.tz; out.vertices[i].tangent.w = rec.tw;
        out.vertices[i].uv0.x = rec.u0; out.vertices[i].uv0.y = rec.v0;
        out.vertices[i].uv1.x = rec.u1; out.vertices[i].uv1.y = rec.v1;
    }

    file.seekg(header.indexOffset);
    if (!ReadBytes(file, out.indices.data(), header.indexCount * sizeof(uint32_t))) return false;

    if (header.boundsOffset) {
        file.seekg(header.boundsOffset);
        BoundsRecord bounds;
        if (ReadBytes(file, &bounds, sizeof(bounds))) {
            out.bounds.min.x = bounds.centerX - bounds.extentsX;
            out.bounds.min.y = bounds.centerY - bounds.extentsY;
            out.bounds.min.z = bounds.centerZ - bounds.extentsZ;
            out.bounds.max.x = bounds.centerX + bounds.extentsX;
            out.bounds.max.y = bounds.centerY + bounds.extentsY;
            out.bounds.max.z = bounds.centerZ + bounds.extentsZ;
        }
    }
    return true;
}

bool MeshReader::Read(const std::vector<uint8_t>& bytes, ImportMesh& out) {
    return ParseMeshBinary(bytes.data(), bytes.size(), out);
}

} // namespace asset
