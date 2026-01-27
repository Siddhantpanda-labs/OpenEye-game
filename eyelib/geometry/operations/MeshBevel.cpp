#include "eyelib/geometry/operations/MeshBevel.h"
#include "eyelib/geometry/core/MeshTopology.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

namespace eyelib {
namespace geometry {

void MeshBevel::BevelEdges(render::Mesh& mesh, const Selection& selection,
                           float amount, int segments, const Mask* mask) {
    if (amount <= 0.0f || segments < 1) return;
    
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
    
    // Build topology
    MeshTopology topology;
    topology.BuildFromIndices(vertices.size(), indices);
    if (!topology.IsValid()) return;
    
    // Determine which edges to bevel
    std::unordered_set<uint32_t> edgesToBevel;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Edges) {
        // Bevel all edges (for testing, but usually you'd want specific edges)
        // For now, skip if no edge selection
        return;
    } else {
        // Bevel only selected edges
        size_t edgeCount = topology.GetEdgeCount();
        for (uint32_t eid : selection.GetIDs()) {
            if (eid < edgeCount) {
                edgesToBevel.insert(eid);
            }
        }
    }
    
    if (edgesToBevel.empty()) return;
    
    // Prepare new geometry
    std::vector<render::Vertex> newVertices = vertices;
    std::vector<uint32_t> newIndices;
    
    // Map: old vertex index -> new vertex index (for beveled vertices)
    std::unordered_map<uint32_t, uint32_t> vertexMap;
    
    // Process each face
    size_t faceCount = indices.size() / 3;
    for (size_t faceIdx = 0; faceIdx < faceCount; ++faceIdx) {
        uint32_t baseIdx = static_cast<uint32_t>(faceIdx * 3);
        uint32_t v0 = indices[baseIdx];
        uint32_t v1 = indices[baseIdx + 1];
        uint32_t v2 = indices[baseIdx + 2];
        
        // Check if any edge of this face is selected for beveling
        bool needsBevel = false;
        uint32_t bevelEdgeIdx = UINT32_MAX;
        
        // Check each edge of the face
        Edge edge0(v0, v1);
        Edge edge1(v1, v2);
        Edge edge2(v2, v0);
        
        uint32_t e0Idx = topology.FindEdge(v0, v1);
        uint32_t e1Idx = topology.FindEdge(v1, v2);
        uint32_t e2Idx = topology.FindEdge(v2, v0);
        
        if (e0Idx != UINT32_MAX && edgesToBevel.find(e0Idx) != edgesToBevel.end()) {
            needsBevel = true;
            bevelEdgeIdx = e0Idx;
        } else if (e1Idx != UINT32_MAX && edgesToBevel.find(e1Idx) != edgesToBevel.end()) {
            needsBevel = true;
            bevelEdgeIdx = e1Idx;
        } else if (e2Idx != UINT32_MAX && edgesToBevel.find(e2Idx) != edgesToBevel.end()) {
            needsBevel = true;
            bevelEdgeIdx = e2Idx;
        }
        
        if (!needsBevel) {
            // Keep original face
            newIndices.push_back(v0);
            newIndices.push_back(v1);
            newIndices.push_back(v2);
            continue;
        }
        
        // For simplicity, implement basic chamfer (segments = 1)
        // Find which edge to bevel
        const auto& edge = topology.GetEdges()[bevelEdgeIdx];
        uint32_t ev0 = edge.v0;
        uint32_t ev1 = edge.v1;
        
        // Get or create beveled vertices
        auto getOrCreateBevelVertex = [&](uint32_t oldVid, uint32_t otherVid) -> uint32_t {
            auto it = vertexMap.find(oldVid);
            if (it != vertexMap.end()) {
                return it->second;
            }
            
            // Compute offset direction (perpendicular to edge, in face plane)
            math::Vec3 edgeDir = glm::normalize(vertices[otherVid].position - vertices[oldVid].position);
            math::Vec3 p0 = vertices[v0].position;
            math::Vec3 p1 = vertices[v1].position;
            math::Vec3 p2 = vertices[v2].position;
            math::Vec3 edge1 = p1 - p0;
            math::Vec3 edge2 = p2 - p0;
            math::Vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));
            if (glm::length(faceNormal) < 0.001f) faceNormal = math::Vec3(0, 1, 0);
            math::Vec3 offsetDir = glm::cross(edgeDir, faceNormal);
            offsetDir = glm::normalize(offsetDir);
            if (glm::length(offsetDir) < 0.001f) offsetDir = math::Vec3(1, 0, 0);
            
            // Apply mask if provided
            float finalAmount = amount;
            if (mask) {
                finalAmount *= mask->GetWeight(oldVid);
            }
            
            // Create new vertex
            render::Vertex newVertex = vertices[oldVid];
            newVertex.position = vertices[oldVid].position + offsetDir * finalAmount;
            
            uint32_t newVid = static_cast<uint32_t>(newVertices.size());
            newVertices.push_back(newVertex);
            vertexMap[oldVid] = newVid;
            
            return newVid;
        };
        
        // For now, implement simple chamfer: replace edge with two triangles
        // This is a simplified version - full bevel would create proper chamfer geometry
        if (segments == 1) {
            // Simple chamfer: create new vertices offset from edge endpoints
            uint32_t bv0 = getOrCreateBevelVertex(ev0, ev1);
            uint32_t bv1 = getOrCreateBevelVertex(ev1, ev0);
            
            // Determine which vertex is not on the beveled edge
            uint32_t vOther = v2;
            if (v0 != ev0 && v0 != ev1) vOther = v0;
            else if (v1 != ev0 && v1 != ev1) vOther = v1;
            
            // Create chamfer faces
            // Face 1: original vertex, bevel vertex 0, bevel vertex 1
            newIndices.push_back(vOther);
            newIndices.push_back(bv0);
            newIndices.push_back(bv1);
            
            // Face 2: bevel vertex 0, original edge vertex 0, bevel vertex 1
            // (This is simplified - proper chamfer needs more geometry)
            newIndices.push_back(bv0);
            newIndices.push_back(ev0);
            newIndices.push_back(bv1);
            
            newIndices.push_back(bv1);
            newIndices.push_back(ev0);
            newIndices.push_back(ev1);
        } else {
            // Multi-segment bevel (rounded) - for now, fall back to chamfer
            // Full implementation would create multiple segments along the edge
            BevelEdges(mesh, selection, amount, 1, mask);
            return;
        }
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

void MeshBevel::BevelVertices(render::Mesh& mesh, const Selection& selection,
                             float amount, int segments, const Mask* mask) {
    // Vertex beveling is more complex - for now, implement basic version
    // Full implementation would create chamfered geometry around selected vertices
    if (amount <= 0.0f || segments < 1) return;
    
    // For now, vertex beveling can be approximated by beveling all edges connected to selected vertices
    // This is a simplified approach - proper vertex beveling requires more sophisticated geometry
    const auto& vertices = mesh.GetVertices();
    if (vertices.empty()) return;
    
    // Get indices
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
    
    if (indices.empty()) return;
    
    // Build topology
    MeshTopology topology;
    topology.BuildFromIndices(vertices.size(), indices);
    if (!topology.IsValid()) return;
    
    // Collect edges connected to selected vertices
    std::unordered_set<uint32_t> edgesToBevel;
    if (!selection.IsEmpty() && selection.GetType() == Selection::Type::Vertices) {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid >= vertices.size()) continue;
            const auto& vertexEdges = topology.GetVertexEdges(vid);
            for (uint32_t eid : vertexEdges) {
                edgesToBevel.insert(eid);
            }
        }
    }
    
    if (edgesToBevel.empty()) return;
    
    // Convert to edge selection and use edge beveling
    std::vector<uint32_t> edgeIDs(edgesToBevel.begin(), edgesToBevel.end());
    Selection edgeSelection = Selection::FromEdgeIDs(edgeIDs);
    BevelEdges(mesh, edgeSelection, amount, segments, mask);
}

void MeshBevel::BevelEdge(render::Mesh& mesh, uint32_t edgeIndex,
                         const std::unordered_set<uint32_t>& selectedEdges,
                         float amount, int segments,
                         const Mask* mask,
                         const MeshTopology& topology,
                         std::vector<render::Vertex>& newVertices,
                         std::vector<uint32_t>& newIndices,
                         std::unordered_map<uint32_t, uint32_t>& vertexMap) {
    // This helper is for more complex beveling - currently handled inline
    // Full implementation would use this for proper multi-segment beveling
}

math::Vec3 MeshBevel::ComputeEdgeOffset(const render::Mesh& mesh, uint32_t v0, uint32_t v1,
                                        const MeshTopology& topology, float amount) {
    const auto& vertices = mesh.GetVertices();
    if (v0 >= vertices.size() || v1 >= vertices.size()) {
        return math::Vec3(0, 0, 0);
    }
    
    // Compute edge direction
    math::Vec3 edgeDir = glm::normalize(vertices[v1].position - vertices[v0].position);
    
    // Get faces sharing this edge
    uint32_t edgeIdx = topology.FindEdge(v0, v1);
    if (edgeIdx == UINT32_MAX) return math::Vec3(0, 0, 0);
    
    const auto& edgeFaces = topology.GetEdgeFaces(edgeIdx);
    if (edgeFaces.empty()) return math::Vec3(0, 0, 0);
    
    // Compute average face normal
    math::Vec3 avgNormal(0.0f);
    for (uint32_t faceIdx : edgeFaces) {
        if (faceIdx >= topology.GetFaceCount()) continue;
        const auto& face = topology.GetFace(faceIdx);
        
        math::Vec3 p0 = vertices[face.v0].position;
        math::Vec3 p1 = vertices[face.v1].position;
        math::Vec3 p2 = vertices[face.v2].position;
        
        math::Vec3 edge1 = p1 - p0;
        math::Vec3 edge2 = p2 - p0;
        math::Vec3 normal = glm::cross(edge1, edge2);
        float len = glm::length(normal);
        if (len > 0.001f) {
            avgNormal += normal / len;
        }
    }
    
    avgNormal /= static_cast<float>(edgeFaces.size());
    avgNormal = glm::normalize(avgNormal);
    
    // Offset direction is perpendicular to edge, in the plane of faces
    math::Vec3 offsetDir = glm::cross(edgeDir, avgNormal);
    offsetDir = glm::normalize(offsetDir);
    
    return offsetDir * amount;
}


} // namespace geometry
} // namespace eyelib
