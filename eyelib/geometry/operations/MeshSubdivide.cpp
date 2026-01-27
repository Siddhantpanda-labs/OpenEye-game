#include "eyelib/geometry/operations/MeshSubdivide.h"
#include "eyelib/geometry/core/MeshTopology.h"
#include <unordered_map>
#include <algorithm>

namespace eyelib {
namespace geometry {

void MeshSubdivide::Subdivide(render::Mesh& mesh, const Selection& selection, int levels) {
    if (levels <= 0) return;
    
    const auto& inputVertices = mesh.GetVertices();
    
    // Get indices (handle both formats)
    std::vector<uint32_t> inputIndices;
    if (mesh.GetIndexFormat() == render::IndexFormat::UInt32) {
        inputIndices = mesh.GetIndices32();
    } else {
        const auto& indices16 = mesh.GetIndices16();
        inputIndices.reserve(indices16.size());
        for (uint16_t idx : indices16) {
            inputIndices.push_back(static_cast<uint32_t>(idx));
        }
    }
    
    if (inputVertices.empty() || inputIndices.empty()) return;
    
    // Determine which faces to subdivide
    std::unordered_set<uint32_t> facesToSubdivide;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Faces) {
        // Subdivide all faces
        size_t faceCount = inputIndices.size() / 3;
        for (size_t i = 0; i < faceCount; ++i) {
            facesToSubdivide.insert(static_cast<uint32_t>(i));
        }
    } else {
        // Subdivide only selected faces
        for (uint32_t fid : selection.GetIDs()) {
            size_t faceCount = inputIndices.size() / 3;
            if (fid < faceCount) {
                facesToSubdivide.insert(fid);
            }
        }
    }
    
    // Perform subdivision levels
    for (int level = 0; level < levels; ++level) {
        std::vector<render::Vertex> newVertices;
        std::vector<uint32_t> newIndices;
        std::unordered_map<uint64_t, uint32_t> edgeMidpoints;
        
        // Copy existing vertices
        newVertices = inputVertices;
        
        // Subdivide each face
        size_t faceCount = inputIndices.size() / 3;
        for (size_t faceIdx = 0; faceIdx < faceCount; ++faceIdx) {
            if (facesToSubdivide.find(static_cast<uint32_t>(faceIdx)) != facesToSubdivide.end()) {
                SubdivideFace(mesh, faceIdx, newVertices, newIndices, edgeMidpoints);
            } else {
                // Keep original face
                uint32_t baseIdx = static_cast<uint32_t>(faceIdx * 3);
                newIndices.push_back(inputIndices[baseIdx]);
                newIndices.push_back(inputIndices[baseIdx + 1]);
                newIndices.push_back(inputIndices[baseIdx + 2]);
            }
        }
        
        // Update mesh
        mesh.SetVertices(newVertices);
        mesh.SetIndices(newIndices);
        
        // Recalculate topology for next level
        if (level < levels - 1) {
            // Rebuild selection for next level (all new faces)
            // For simplicity, we'll subdivide all faces in subsequent levels
            facesToSubdivide.clear();
            faceCount = newIndices.size() / 3;
            for (size_t i = 0; i < faceCount; ++i) {
                facesToSubdivide.insert(static_cast<uint32_t>(i));
            }
        }
    }
    
    // Finalize mesh
    mesh.GenerateNormals();
    mesh.GenerateTangents();
    mesh.InvalidateAABB();
    mesh.UploadToGPU();
}

void MeshSubdivide::SubdivideFace(const render::Mesh& inputMesh, size_t faceIndex,
                                 std::vector<render::Vertex>& newVertices,
                                 std::vector<uint32_t>& newIndices,
                                 std::unordered_map<uint64_t, uint32_t>& edgeMidpoints) {
    const auto& inputVertices = inputMesh.GetVertices();
    
    // Get indices (handle both formats)
    std::vector<uint32_t> inputIndices;
    if (inputMesh.GetIndexFormat() == render::IndexFormat::UInt32) {
        inputIndices = inputMesh.GetIndices32();
    } else {
        const auto& indices16 = inputMesh.GetIndices16();
        inputIndices.reserve(indices16.size());
        for (uint16_t idx : indices16) {
            inputIndices.push_back(static_cast<uint32_t>(idx));
        }
    }
    
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
        
        render::Vertex midpoint;
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

void MeshSubdivide::SubdivideLoop(render::Mesh& mesh, const Selection& selection, int levels) {
    // For now, use simple subdivision
    // Full Loop subdivision would require edge-based topology and special rules
    Subdivide(mesh, selection, levels);
}

uint64_t MeshSubdivide::GetEdgeKey(uint32_t v0, uint32_t v1) {
    if (v0 > v1) std::swap(v0, v1);
    return (static_cast<uint64_t>(v0) << 32) | static_cast<uint64_t>(v1);
}

} // namespace geometry
} // namespace eyelib
