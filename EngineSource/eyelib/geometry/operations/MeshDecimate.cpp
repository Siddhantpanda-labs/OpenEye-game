#include "eyelib/geometry/operations/MeshDecimate.h"
#include "eyelib/geometry/core/MeshTopology.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/MeshDescriptorUtils.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <cmath>
#include <functional>

namespace eyelib {
namespace geometry {

void MeshDecimate::Decimate(geometry::EditableMesh& editable, size_t targetTriangleCount,
                            const Selection& selection, const Mask* mask) {
    const auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    // Get indices
    std::vector<uint32_t> indices = editable.GetIndices();
    
    if (indices.empty() || indices.size() % 3 != 0) return;
    
    size_t currentTriangleCount = indices.size() / 3;
    if (targetTriangleCount >= currentTriangleCount) return; // Nothing to do
    
    // Build topology
    MeshTopology topology;
    topology.BuildFromIndices(vertices.size(), indices);
    if (!topology.IsValid()) return;
    
    // Determine which faces can be decimated
    std::unordered_set<uint32_t> facesToDecimate;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Faces) {
        // Decimate all faces
        for (size_t i = 0; i < currentTriangleCount; ++i) {
            facesToDecimate.insert(static_cast<uint32_t>(i));
        }
    } else {
        // Decimate only selected faces
        for (uint32_t fid : selection.GetIDs()) {
            if (fid < currentTriangleCount) {
                facesToDecimate.insert(fid);
            }
        }
    }
    
    if (facesToDecimate.empty()) return;
    
    // Collect edges from faces to decimate
    std::unordered_set<uint32_t> verticesInRegion;
    for (uint32_t fid : facesToDecimate) {
        uint32_t baseIdx = fid * 3;
        if (baseIdx + 2 < indices.size()) {
            verticesInRegion.insert(indices[baseIdx]);
            verticesInRegion.insert(indices[baseIdx + 1]);
            verticesInRegion.insert(indices[baseIdx + 2]);
        }
    }
    
    // Priority queue for edge collapses (lowest error first)
    // Custom comparator for min-heap (lowest error at top)
    auto cmp = [](const EdgeCollapse& a, const EdgeCollapse& b) {
        return a.error > b.error; // Reverse for min-heap
    };
    std::priority_queue<EdgeCollapse, std::vector<EdgeCollapse>, decltype(cmp)> collapseQueue(cmp);
    
    // Compute error for all edges in the region
    for (uint32_t vid : verticesInRegion) {
        const auto& vertexEdges = topology.GetVertexEdges(vid);
        for (uint32_t eid : vertexEdges) {
            if (eid >= topology.GetEdgeCount()) continue;
            const auto& edge = topology.GetEdges()[eid];
            
            // Only consider edges where both vertices are in region
            if (verticesInRegion.find(edge.v0) == verticesInRegion.end() ||
                verticesInRegion.find(edge.v1) == verticesInRegion.end()) {
                continue;
            }
            
            // Check if edge can be collapsed
            if (!CanCollapseEdge(edge.v0, edge.v1, topology)) continue;
            
            // Compute error
            math::Vec3 optimalPos;
            float error = ComputeEdgeError(editable, edge.v0, edge.v1, topology, optimalPos);
            
            // Apply mask if provided
            if (mask) {
                float maskWeight0 = mask->GetWeight(edge.v0);
                float maskWeight1 = mask->GetWeight(edge.v1);
                if (maskWeight0 < 0.5f || maskWeight1 < 0.5f) {
                    continue; // Skip edges with protected vertices
                }
            }
            
            EdgeCollapse collapse;
            collapse.v0 = edge.v0;
            collapse.v1 = edge.v1;
            collapse.error = error;
            collapse.newPosition = optimalPos;
            
            collapseQueue.push(collapse);
        }
    }
    
    // Perform edge collapses until target triangle count is reached
    size_t trianglesToRemove = currentTriangleCount - targetTriangleCount;
    std::vector<render::Vertex> newVertices = vertices;
    std::vector<uint32_t> newIndices = indices;
    std::unordered_map<uint32_t, uint32_t> vertexMap; // Old -> New mapping
    
    size_t collapsesPerformed = 0;
    size_t maxCollapses = trianglesToRemove; // Each collapse removes ~2 triangles on average
    
    while (!collapseQueue.empty() && collapsesPerformed < maxCollapses) {
        EdgeCollapse collapse = collapseQueue.top();
        collapseQueue.pop();
        
        // Check if vertices still exist (may have been collapsed already)
        if (collapse.v0 >= newVertices.size() || collapse.v1 >= newVertices.size()) continue;
        
        // Check if edge still exists and can be collapsed
        if (!CanCollapseEdge(collapse.v0, collapse.v1, topology)) continue;
        
        // Perform collapse
        CollapseEdge(editable, collapse.v0, collapse.v1, topology, newVertices, newIndices, vertexMap);
        
        // Rebuild topology for next iteration
        topology.BuildFromIndices(newVertices.size(), newIndices);
        if (!topology.IsValid()) break;
        
        collapsesPerformed++;
        
        // Check if we've reached target triangle count
        size_t currentTriCount = newIndices.size() / 3;
        if (currentTriCount <= targetTriangleCount) break;
    }
    
    // Update mesh
    editable.SetVertices(std::move(newVertices));
    editable.SetIndices(std::move(newIndices));
    auto desc = editable.GetDescriptor();
    render::MeshDescriptorUtils::GenerateNormals(desc);
    render::MeshDescriptorUtils::GenerateTangents(desc);
    editable.SetVertices(std::move(desc.vertices));
}

void MeshDecimate::DecimateByPercentage(geometry::EditableMesh& editable, float reductionFactor,
                                        const Selection& selection, const Mask* mask) {
    reductionFactor = std::clamp(reductionFactor, 0.0f, 1.0f);
    
    // Get current triangle count
    const auto& indices = editable.GetIndices();
    
    if (indices.empty() || indices.size() % 3 != 0) return;
    
    size_t currentTriangleCount = indices.size() / 3;
    size_t targetTriangleCount = static_cast<size_t>(currentTriangleCount * (1.0f - reductionFactor));
    
    Decimate(editable, targetTriangleCount, selection, mask);
}

float MeshDecimate::ComputeEdgeError(const geometry::EditableMesh& editable, uint32_t v0, uint32_t v1,
                                    const MeshTopology& topology, math::Vec3& optimalPos) {
    const auto& vertices = editable.GetVertices();
    if (v0 >= vertices.size() || v1 >= vertices.size()) {
        optimalPos = vertices[std::min(v0, v1)].position;
        return 1000000.0f; // High error for invalid edges
    }
    
    // Simple error metric: edge length (shorter edges = lower error)
    // Optimal position: midpoint (can be improved with quadric error metrics)
    math::Vec3 p0 = vertices[v0].position;
    math::Vec3 p1 = vertices[v1].position;
    
    optimalPos = (p0 + p1) * 0.5f;
    
    float edgeLength = glm::length(p1 - p0);
    
    // Add penalty for boundary edges (prefer collapsing interior edges)
    uint32_t edgeIdx = topology.FindEdge(v0, v1);
    if (edgeIdx != UINT32_MAX) {
        const auto& edgeFaces = topology.GetEdgeFaces(edgeIdx);
        if (edgeFaces.size() == 1) {
            edgeLength *= 2.0f; // Penalize boundary edges
        }
    }
    
    return edgeLength;
}

bool MeshDecimate::CanCollapseEdge(uint32_t v0, uint32_t v1, const MeshTopology& topology) {
    // Check if edge exists
    uint32_t edgeIdx = topology.FindEdge(v0, v1);
    if (edgeIdx == UINT32_MAX) return false;
    
    // Get faces sharing this edge
    const auto& edgeFaces = topology.GetEdgeFaces(edgeIdx);
    if (edgeFaces.empty()) return false;
    
    // Get faces of both vertices
    const auto& facesV0 = topology.GetVertexFaces(v0);
    const auto& facesV1 = topology.GetVertexFaces(v1);
    
    // Check for fold-over: if v0 and v1 share more than 2 faces, collapsing would create invalid geometry
    // This is a simplified check - full implementation would check for manifold constraints
    
    // Basic check: ensure both vertices have faces
    if (facesV0.empty() || facesV1.empty()) return false;
    
    return true;
}

void MeshDecimate::CollapseEdge(const geometry::EditableMesh& editable, uint32_t v0, uint32_t v1,
                                const MeshTopology& topology,
                                std::vector<render::Vertex>& newVertices,
                                std::vector<uint32_t>& newIndices,
                                std::unordered_map<uint32_t, uint32_t>& vertexMap) {
    // Collapse v1 into v0 (merge v1 into v0)
    // Update v0 position to optimal collapse position
    math::Vec3 optimalPos;
    ComputeEdgeError(editable, v0, v1, topology, optimalPos);
    
    if (v0 >= newVertices.size() || v1 >= newVertices.size()) return;
    
    // Update v0 position
    newVertices[v0].position = optimalPos;
    
    // Update all indices: replace v1 with v0
    for (size_t i = 0; i < newIndices.size(); ++i) {
        if (newIndices[i] == v1) {
            newIndices[i] = v0;
        }
    }
    
    // Remove degenerate triangles (where all vertices are the same)
    std::vector<uint32_t> cleanedIndices;
    for (size_t i = 0; i < newIndices.size(); i += 3) {
        if (i + 2 >= newIndices.size()) break;
        
        uint32_t tv0 = newIndices[i];
        uint32_t tv1 = newIndices[i + 1];
        uint32_t tv2 = newIndices[i + 2];
        
        // Skip degenerate triangles
        if (tv0 == tv1 || tv1 == tv2 || tv2 == tv0) {
            continue;
        }
        
        cleanedIndices.push_back(tv0);
        cleanedIndices.push_back(tv1);
        cleanedIndices.push_back(tv2);
    }
    
    newIndices = cleanedIndices;
    
    // Note: In a full implementation, we would also remove vertex v1 from newVertices
    // and remap all indices. For simplicity, we keep it but it won't be referenced.
}

} // namespace geometry
} // namespace eyelib
