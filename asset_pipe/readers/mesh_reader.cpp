#include "asset_pipe/readers/mesh_reader.h"
#include "asset_pipe/core/mesh_format.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

namespace asset_pipe {

namespace {

using namespace MeshFormat;

constexpr size_t kAlign = kAlignment;

size_t AlignUp(size_t value, size_t align) {
    return (value + align - 1) & ~(align - 1);
}

bool ReadBytes(std::istream& in, void* data, size_t size) {
    return in.read(static_cast<char*>(data), static_cast<std::streamsize>(size)).good();
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

} // namespace asset_pipe
