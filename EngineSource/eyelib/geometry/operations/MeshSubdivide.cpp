#include "eyelib/geometry/operations/MeshSubdivide.h"
#include "eyelib/geometry/core/MeshTopology.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/MeshDescriptorUtils.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace eyelib {
namespace geometry {

void MeshSubdivide::Subdivide(geometry::EditableMesh& editable, const Selection& selection, int levels) {
    if (levels <= 0) return;
    
    if (editable.GetVertices().empty() || editable.GetIndices().empty()) return;
    
    for (int level = 0; level < levels; ++level) {
        const auto& inputVertices = editable.GetVertices();
        const auto& inputIndices = editable.GetIndices();
        if (inputVertices.empty() || inputIndices.empty()) break;
        
        std::unordered_set<uint32_t> facesToSubdivide;
        if (level == 0 && !selection.IsEmpty() && selection.GetType() == Selection::Type::Faces) {
            size_t faceCount = inputIndices.size() / 3;
            for (uint32_t fid : selection.GetIDs()) {
                if (fid < faceCount) facesToSubdivide.insert(fid);
            }
        }
        if (facesToSubdivide.empty()) {
            size_t faceCount = inputIndices.size() / 3;
            for (size_t i = 0; i < faceCount; ++i) facesToSubdivide.insert(static_cast<uint32_t>(i));
        }
        
        std::vector<geometry::EditableMesh::Vertex> newVertices = inputVertices;
        std::vector<uint32_t> newIndices;
        newIndices.reserve(inputIndices.size() * 4);
        std::unordered_map<uint64_t, uint32_t> edgeMidpoints;
        
        size_t faceCount = inputIndices.size() / 3;
        for (size_t faceIdx = 0; faceIdx < faceCount; ++faceIdx) {
            if (facesToSubdivide.count(static_cast<uint32_t>(faceIdx))) {
                SubdivideFace(editable, faceIdx, newVertices, newIndices, edgeMidpoints);
            } else {
                uint32_t baseIdx = static_cast<uint32_t>(faceIdx * 3);
                newIndices.push_back(inputIndices[baseIdx]);
                newIndices.push_back(inputIndices[baseIdx + 1]);
                newIndices.push_back(inputIndices[baseIdx + 2]);
            }
        }
        
        editable.SetVertices(std::move(newVertices));
        editable.SetIndices(std::move(newIndices));
    }
    
    auto desc = editable.GetDescriptor();
    render::MeshDescriptorUtils::GenerateNormals(desc);
    editable.SetVertices(std::move(desc.vertices));
    desc = editable.GetDescriptor();
    render::MeshDescriptorUtils::GenerateTangents(desc);
    editable.SetVertices(std::move(desc.vertices));
}

void MeshSubdivide::SubdivideFace(const geometry::EditableMesh& inputMesh, size_t faceIndex,
                                 std::vector<geometry::EditableMesh::Vertex>& newVertices,
                                 std::vector<uint32_t>& newIndices,
                                 std::unordered_map<uint64_t, uint32_t>& edgeMidpoints) {
    const auto& inputVertices = inputMesh.GetVertices();
    const auto& inputIndices = inputMesh.GetIndices();
    
    if (faceIndex * 3 + 2 >= inputIndices.size()) return;
    
    uint32_t v0 = inputIndices[faceIndex * 3];
    uint32_t v1 = inputIndices[faceIndex * 3 + 1];
    uint32_t v2 = inputIndices[faceIndex * 3 + 2];
    
    if (v0 >= inputVertices.size() || v1 >= inputVertices.size() || v2 >= inputVertices.size()) {
        return;
    }
    
    // Get or create edge midpoints
    auto getMidpoint = [&](uint32_t a, uint32_t b) -> uint32_t {
        uint64_t key = GetEdgeKey(a, b);
        auto it = edgeMidpoints.find(key);
        if (it != edgeMidpoints.end()) {
            return it->second;
        }
        
        // Create new midpoint vertex
        const auto& va = inputVertices[a];
        const auto& vb = inputVertices[b];
        
        geometry::EditableMesh::Vertex midpoint;
        midpoint.position = (va.position + vb.position) * 0.5f;
        midpoint.normal = glm::normalize((va.normal + vb.normal) * 0.5f);
        midpoint.uv = (va.uv + vb.uv) * 0.5f;
        midpoint.color = (va.color + vb.color) * 0.5f;
        
        uint32_t midIdx = static_cast<uint32_t>(newVertices.size());
        newVertices.push_back(midpoint);
        edgeMidpoints[key] = midIdx;
        return midIdx;
    };
    
    uint32_t m01 = getMidpoint(v0, v1);
    uint32_t m12 = getMidpoint(v1, v2);
    uint32_t m20 = getMidpoint(v2, v0);
    
    // Create 4 new triangles
    // Triangle 1: v0, m01, m20
    newIndices.push_back(v0);
    newIndices.push_back(m01);
    newIndices.push_back(m20);
    
    // Triangle 2: m01, v1, m12
    newIndices.push_back(m01);
    newIndices.push_back(v1);
    newIndices.push_back(m12);
    
    // Triangle 3: m20, m12, v2
    newIndices.push_back(m20);
    newIndices.push_back(m12);
    newIndices.push_back(v2);
    
    // Triangle 4: m01, m12, m20 (center)
    newIndices.push_back(m01);
    newIndices.push_back(m12);
    newIndices.push_back(m20);
}

void MeshSubdivide::SubdivideLoop(geometry::EditableMesh& editable, const Selection& selection, int levels) {
    Subdivide(editable, selection, levels);
}

uint64_t MeshSubdivide::GetEdgeKey(uint32_t v0, uint32_t v1) {
    if (v0 > v1) std::swap(v0, v1);
    return (static_cast<uint64_t>(v0) << 32) | static_cast<uint64_t>(v1);
}

} // namespace geometry
} // namespace eyelib
