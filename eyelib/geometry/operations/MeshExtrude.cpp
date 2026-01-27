#include "eyelib/geometry/operations/MeshExtrude.h"
#include "eyelib/geometry/core/MeshTopology.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace eyelib {
namespace geometry {

void MeshExtrude::Extrude(render::Mesh& mesh, const Selection& selection,
                         float amount, const Mask* mask) {
    if (amount == 0.0f) return;
    
    const auto& vertices = mesh.GetVertices();
    if (vertices.empty()) return;
    
    // Get indices (handle both formats)
    std::vector<uint32_t> indices;
    if (mesh.GetIndexFormat() == render::IndexFormat::UInt32) {
        indices = mesh.GetIndices32();
    } else {
        const auto& indices16 = mesh.GetIndices16();
        indices.reserve(indices16.size());
        for (uint16_t idx : indices16) {
            indices.push_back(static_cast<uint32_t>(idx));
        }
    }
    
    if (indices.empty() || indices.size() % 3 != 0) return;
    
    // Determine which faces to extrude
    std::unordered_set<uint32_t> facesToExtrude;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Faces) {
        // Extrude all faces
        size_t faceCount = indices.size() / 3;
        for (size_t i = 0; i < faceCount; ++i) {
            facesToExtrude.insert(static_cast<uint32_t>(i));
        }
    } else {
        // Extrude only selected faces
        size_t faceCount = indices.size() / 3;
        for (uint32_t fid : selection.GetIDs()) {
            if (fid < faceCount) {
                facesToExtrude.insert(fid);
            }
        }
    }
    
    if (facesToExtrude.empty()) return;
    
    // Build topology to find adjacent faces
    MeshTopology topology;
    topology.BuildFromIndices(vertices.size(), indices);
    if (!topology.IsValid()) return;
    
    // Prepare new geometry
    std::vector<render::Vertex> newVertices = vertices;
    std::vector<uint32_t> newIndices;
    
    // Map: old vertex index -> new vertex index (for extruded vertices)
    std::unordered_map<uint32_t, uint32_t> vertexMap;
    
    // Process each face to extrude
    size_t faceCount = indices.size() / 3;
    for (size_t faceIdx = 0; faceIdx < faceCount; ++faceIdx) {
        uint32_t fid = static_cast<uint32_t>(faceIdx);
        
        if (facesToExtrude.find(fid) == facesToExtrude.end()) {
            // Keep original face
            uint32_t baseIdx = static_cast<uint32_t>(faceIdx * 3);
            newIndices.push_back(indices[baseIdx]);
            newIndices.push_back(indices[baseIdx + 1]);
            newIndices.push_back(indices[baseIdx + 2]);
            continue;
        }
        
        // Get face vertices
        uint32_t baseIdx = static_cast<uint32_t>(faceIdx * 3);
        uint32_t v0 = indices[baseIdx];
        uint32_t v1 = indices[baseIdx + 1];
        uint32_t v2 = indices[baseIdx + 2];
        
        // Compute face normal
        math::Vec3 faceNormal = ComputeFaceNormal(mesh, v0, v1, v2);
        if (glm::length(faceNormal) < 0.001f) {
            // Degenerate face, skip
            newIndices.push_back(v0);
            newIndices.push_back(v1);
            newIndices.push_back(v2);
            continue;
        }
        
        // Keep original face first
        newIndices.push_back(v0);
        newIndices.push_back(v1);
        newIndices.push_back(v2);
        
        // Extrude this face (creates new top face and side faces)
        ExtrudeFace(mesh, fid, facesToExtrude, amount, faceNormal, mask,
                   newVertices, newIndices, vertexMap);
    }
    
    // Update mesh
    mesh.SetVertices(newVertices);
    mesh.SetIndices(newIndices);
    
    // Recalculate normals and update GPU
    mesh.GenerateNormals();
    mesh.GenerateTangents();
    mesh.InvalidateAABB();
    mesh.UploadToGPU();
}

void MeshExtrude::ExtrudeAlongDirection(render::Mesh& mesh, const Selection& selection,
                                       const math::Vec3& direction, float amount,
                                       const Mask* mask) {
    if (amount == 0.0f) return;
    
    math::Vec3 normalizedDir = glm::normalize(direction);
    if (glm::length(normalizedDir) < 0.001f) {
        normalizedDir = math::Vec3(0, 1, 0); // Fallback
    }
    
    const auto& vertices = mesh.GetVertices();
    if (vertices.empty()) return;
    
    // Get indices (handle both formats)
    std::vector<uint32_t> indices;
    if (mesh.GetIndexFormat() == render::IndexFormat::UInt32) {
        indices = mesh.GetIndices32();
    } else {
        const auto& indices16 = mesh.GetIndices16();
        indices.reserve(indices16.size());
        for (uint16_t idx : indices16) {
            indices.push_back(static_cast<uint32_t>(idx));
        }
    }
    
    if (indices.empty() || indices.size() % 3 != 0) return;
    
    // Determine which faces to extrude
    std::unordered_set<uint32_t> facesToExtrude;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Faces) {
        size_t faceCount = indices.size() / 3;
        for (size_t i = 0; i < faceCount; ++i) {
            facesToExtrude.insert(static_cast<uint32_t>(i));
        }
    } else {
        size_t faceCount = indices.size() / 3;
        for (uint32_t fid : selection.GetIDs()) {
            if (fid < faceCount) {
                facesToExtrude.insert(fid);
            }
        }
    }
    
    if (facesToExtrude.empty()) return;
    
    // Build topology
    MeshTopology topology;
    topology.BuildFromIndices(vertices.size(), indices);
    if (!topology.IsValid()) return;
    
    // Prepare new geometry
    std::vector<render::Vertex> newVertices = vertices;
    std::vector<uint32_t> newIndices;
    std::unordered_map<uint32_t, uint32_t> vertexMap;
    
    // Process each face
    size_t faceCount = indices.size() / 3;
    for (size_t faceIdx = 0; faceIdx < faceCount; ++faceIdx) {
        uint32_t fid = static_cast<uint32_t>(faceIdx);
        
        if (facesToExtrude.find(fid) == facesToExtrude.end()) {
            uint32_t baseIdx = static_cast<uint32_t>(faceIdx * 3);
            newIndices.push_back(indices[baseIdx]);
            newIndices.push_back(indices[baseIdx + 1]);
            newIndices.push_back(indices[baseIdx + 2]);
            continue;
        }
        
        ExtrudeFace(mesh, fid, facesToExtrude, amount, normalizedDir, mask,
                   newVertices, newIndices, vertexMap);
    }
    
    // Update mesh
    mesh.SetVertices(newVertices);
    mesh.SetIndices(newIndices);
    
    // Recalculate normals and update GPU
    mesh.GenerateNormals();
    mesh.GenerateTangents();
    mesh.InvalidateAABB();
    mesh.UploadToGPU();
}

math::Vec3 MeshExtrude::ComputeFaceNormal(const render::Mesh& mesh, uint32_t v0, uint32_t v1, uint32_t v2) {
    const auto& vertices = mesh.GetVertices();
    if (v0 >= vertices.size() || v1 >= vertices.size() || v2 >= vertices.size()) {
        return math::Vec3(0, 1, 0);
    }
    
    math::Vec3 p0 = vertices[v0].position;
    math::Vec3 p1 = vertices[v1].position;
    math::Vec3 p2 = vertices[v2].position;
    
    math::Vec3 edge1 = p1 - p0;
    math::Vec3 edge2 = p2 - p0;
    math::Vec3 normal = glm::cross(edge1, edge2);
    
    float len = glm::length(normal);
    if (len > 0.001f) {
        return normal / len;
    }
    
    return math::Vec3(0, 1, 0);
}

void MeshExtrude::ExtrudeFace(render::Mesh& mesh, uint32_t faceIndex,
                              const std::unordered_set<uint32_t>& selectedFaces,
                              float amount, const math::Vec3& direction,
                              const Mask* mask,
                              std::vector<render::Vertex>& newVertices,
                              std::vector<uint32_t>& newIndices,
                              std::unordered_map<uint32_t, uint32_t>& vertexMap) {
    const auto& vertices = mesh.GetVertices();
    
    // Get indices (handle both formats)
    std::vector<uint32_t> indices;
    if (mesh.GetIndexFormat() == render::IndexFormat::UInt32) {
        indices = mesh.GetIndices32();
    } else {
        const auto& indices16 = mesh.GetIndices16();
        indices.reserve(indices16.size());
        for (uint16_t idx : indices16) {
            indices.push_back(static_cast<uint32_t>(idx));
        }
    }
    
    uint32_t baseIdx = faceIndex * 3;
    if (baseIdx + 2 >= indices.size()) return;
    
    uint32_t v0 = indices[baseIdx];
    uint32_t v1 = indices[baseIdx + 1];
    uint32_t v2 = indices[baseIdx + 2];
    
    if (v0 >= vertices.size() || v1 >= vertices.size() || v2 >= vertices.size()) return;
    
    // Get or create extruded vertices
    auto getOrCreateExtrudedVertex = [&](uint32_t oldVid) -> uint32_t {
        auto it = vertexMap.find(oldVid);
        if (it != vertexMap.end()) {
            return it->second;
        }
        
        // Create new vertex
        render::Vertex newVertex = vertices[oldVid];
        
        // Apply mask if provided
        float finalAmount = amount;
        if (mask) {
            finalAmount *= mask->GetWeight(oldVid);
        }
        
        // Move vertex along direction
        newVertex.position = vertices[oldVid].position + direction * finalAmount;
        
        // Add to new vertices
        uint32_t newVid = static_cast<uint32_t>(newVertices.size());
        newVertices.push_back(newVertex);
        vertexMap[oldVid] = newVid;
        
        return newVid;
    };
    
    uint32_t ev0 = getOrCreateExtrudedVertex(v0);
    uint32_t ev1 = getOrCreateExtrudedVertex(v1);
    uint32_t ev2 = getOrCreateExtrudedVertex(v2);
    
    // Keep original face (it stays in place)
    // Note: Original face indices are already added in the calling function
    
    // Create new top face (using extruded vertices, reversed winding for correct normal)
    newIndices.push_back(ev0);
    newIndices.push_back(ev2);
    newIndices.push_back(ev1);
    
    // Create side faces (quads as two triangles)
    // Side 1: v0-v1-ev1-ev0
    newIndices.push_back(v0);
    newIndices.push_back(ev0);
    newIndices.push_back(v1);
    
    newIndices.push_back(v1);
    newIndices.push_back(ev0);
    newIndices.push_back(ev1);
    
    // Side 2: v1-v2-ev2-ev1
    newIndices.push_back(v1);
    newIndices.push_back(ev1);
    newIndices.push_back(v2);
    
    newIndices.push_back(v2);
    newIndices.push_back(ev1);
    newIndices.push_back(ev2);
    
    // Side 3: v2-v0-ev0-ev2
    newIndices.push_back(v2);
    newIndices.push_back(ev2);
    newIndices.push_back(v0);
    
    newIndices.push_back(v0);
    newIndices.push_back(ev2);
    newIndices.push_back(ev0);
}

} // namespace geometry
} // namespace eyelib
