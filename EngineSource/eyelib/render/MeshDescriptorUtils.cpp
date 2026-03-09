#include "eyelib/render/MeshDescriptorUtils.h"
#include "eyelib/render/MeshDescriptor.h"
#include <algorithm>
#include <cmath>

namespace eyelib {
namespace render {

namespace {

static void accumulateNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t a = indices[i], b = indices[i + 1], c = indices[i + 2];
        if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) continue;
        math::Vec3 p0 = vertices[a].position, p1 = vertices[b].position, p2 = vertices[c].position;
        math::Vec3 e1 = p1 - p0, e2 = p2 - p0;
        math::Vec3 n = glm::cross(e1, e2);
        float len = glm::length(n);
        if (len > 1e-7f) n /= len;
        vertices[a].normal += n;
        vertices[b].normal += n;
        vertices[c].normal += n;
    }
    for (auto& v : vertices) {
        float len = glm::length(v.normal);
        if (len > 1e-7f) v.normal /= len;
        else v.normal = math::Vec3(0, 1, 0);
    }
}

static void accumulateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t a = indices[i], b = indices[i + 1], c = indices[i + 2];
        if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) continue;
        math::Vec3 p0 = vertices[a].position, p1 = vertices[b].position, p2 = vertices[c].position;
        math::Vec2 uv0 = vertices[a].uv, uv1 = vertices[b].uv, uv2 = vertices[c].uv;
        math::Vec3 e1 = p1 - p0, e2 = p2 - p0;
        math::Vec2 d1 = uv1 - uv0, d2 = uv2 - uv0;
        float denom = d1.x * d2.y - d2.x * d1.y;
        if (std::fabs(denom) < 1e-7f) continue;
        float r = 1.0f / denom;
        math::Vec3 t = (e1 * d2.y - e2 * d1.y) * r;
        vertices[a].tangent += t;
        vertices[b].tangent += t;
        vertices[c].tangent += t;
    }
    for (auto& v : vertices) {
        math::Vec3 n = v.normal;
        math::Vec3 t = v.tangent;
        t = t - n * glm::dot(n, t);
        float len = glm::length(t);
        if (len > 1e-7f) v.tangent = t / len;
        else v.tangent = math::Vec3(1, 0, 0);
    }
}

static void optimizeVertexCache(std::vector<uint32_t>& indices) {
    (void)indices;
    /* Placeholder: full tipsify would go here. */
}

} // namespace

void MeshDescriptorUtils::GenerateNormals(MeshDescriptor& desc) {
    const std::vector<uint32_t>* inds = !desc.indices32.empty() ? &desc.indices32 : nullptr;
    std::vector<uint32_t> indices32From16;
    if (!inds && !desc.indices16.empty()) {
        indices32From16.reserve(desc.indices16.size());
        for (uint16_t i : desc.indices16) indices32From16.push_back(static_cast<uint32_t>(i));
        inds = &indices32From16;
    }
    if (!inds || inds->empty()) return;
    for (auto& v : desc.vertices) v.normal = math::Vec3(0, 0, 0);
    accumulateNormals(desc.vertices, *inds);
}

void MeshDescriptorUtils::GenerateTangents(MeshDescriptor& desc) {
    if (!desc.indices32.empty()) {
        for (auto& v : desc.vertices) v.tangent = math::Vec3(0, 0, 0);
        accumulateTangents(desc.vertices, desc.indices32);
    } else if (!desc.indices16.empty()) {
        std::vector<uint32_t> inds(desc.indices16.begin(), desc.indices16.end());
        for (auto& v : desc.vertices) v.tangent = math::Vec3(0, 0, 0);
        accumulateTangents(desc.vertices, inds);
    }
}

void MeshDescriptorUtils::Optimize(MeshDescriptor& desc) {
    if (!desc.indices32.empty()) optimizeVertexCache(desc.indices32);
}

math::AABB MeshDescriptorUtils::ComputeAABB(const MeshDescriptor& desc) {
    math::AABB aabb;
    if (desc.vertices.empty()) return aabb;
    for (const auto& v : desc.vertices)
        aabb.Expand(v.position);
    return aabb;
}

} // namespace render
} // namespace eyelib
