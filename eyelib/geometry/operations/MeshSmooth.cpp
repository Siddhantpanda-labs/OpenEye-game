#include "eyelib/geometry/operations/MeshSmooth.h"
#include "eyelib/geometry/core/MeshTopology.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace eyelib {
namespace geometry {

void MeshSmooth::Smooth(render::Mesh& mesh, const Selection& selection, 
                       float strength, int iterations, const Mask* mask) {
    SmoothUniform(mesh, selection, strength, iterations, mask);
}

void MeshSmooth::SmoothUniform(render::Mesh& mesh, const Selection& selection,
                               float strength, int iterations, const Mask* mask) {
    if (iterations <= 0 || strength <= 0.0f) return;
    
    auto& vertices = mesh.GetVertices();
    if (vertices.empty()) return;
    
    // Build topology for neighbor queries
    MeshTopology topology;
    std::vector<uint32_t> indices;
    if (mesh.GetIndexFormat() == render::IndexFormat::UInt32) {
        indices = mesh.GetIndices32();
    } else {
        // Convert 16-bit to 32-bit
        const auto& indices16 = mesh.GetIndices16();
        indices.reserve(indices16.size());
        for (uint16_t idx : indices16) {
            indices.push_back(static_cast<uint32_t>(idx));
        }
    }
    if (indices.empty()) return;
    
    topology.BuildFromIndices(vertices.size(), indices);
    if (!topology.IsValid()) return;
    
    // Determine which vertices to smooth
    std::unordered_set<uint32_t> verticesToSmooth;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        // Smooth all vertices
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToSmooth.insert(static_cast<uint32_t>(i));
        }
    } else {
        // Smooth only selected vertices
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToSmooth.insert(vid);
            }
        }
    }
    
    // Perform smoothing iterations
    for (int iter = 0; iter < iterations; ++iter) {
        std::vector<math::Vec3> newPositions(vertices.size());
        
        // Copy current positions
        for (size_t i = 0; i < vertices.size(); ++i) {
            newPositions[i] = vertices[i].position;
        }
        
        // Smooth each vertex
        for (uint32_t vid : verticesToSmooth) {
            if (vid >= vertices.size()) continue;
            
            // Get neighbors using the already-built topology
            auto neighbors = GetVertexNeighbors(topology, vid, vertices.size());
            if (neighbors.empty()) continue;
            
            // Compute average neighbor position
            math::Vec3 avgNeighbor(0.0f);
            for (uint32_t nid : neighbors) {
                if (nid < vertices.size()) {
                    avgNeighbor += vertices[nid].position;
                }
            }
            avgNeighbor /= static_cast<float>(neighbors.size());
            
            // Blend between current and average
            math::Vec3 currentPos = vertices[vid].position;
            math::Vec3 smoothedPos = currentPos + (avgNeighbor - currentPos) * strength;
            
            // Apply mask if provided
            float finalStrength = strength;
            if (mask) {
                finalStrength *= mask->GetWeight(vid);
            }
            
            newPositions[vid] = currentPos + (smoothedPos - currentPos) * finalStrength;
        }
        
        // Update positions
        for (size_t i = 0; i < vertices.size(); ++i) {
            vertices[i].position = newPositions[i];
        }
    }
    
    // Recalculate normals and update GPU
    mesh.GenerateNormals();
    mesh.InvalidateAABB();
    mesh.UploadToGPU();
}

void MeshSmooth::SmoothCotangent(render::Mesh& mesh, const Selection& selection,
                                 float strength, int iterations, const Mask* mask) {
    // For now, use uniform smoothing
    // Full cotangent implementation would require edge length calculations
    SmoothUniform(mesh, selection, strength, iterations, mask);
}

std::vector<uint32_t> MeshSmooth::GetVertexNeighbors(const MeshTopology& topology, uint32_t vertexID, size_t vertexCount) {
    std::vector<uint32_t> neighbors;
    std::unordered_set<uint32_t> neighborSet;
    
    if (!topology.IsValid() || vertexID >= vertexCount) {
        return neighbors;
    }
    
    // Get faces connected to this vertex
    const auto& faces = topology.GetVertexFaces(vertexID);
    
    // Collect neighbors from faces
    size_t faceCount = topology.GetFaceCount();
    for (uint32_t faceIdx : faces) {
        // Bounds check to prevent crash
        if (faceIdx >= faceCount) continue;
        
        const auto& face = topology.GetFace(faceIdx);
        
        // Validate face vertex indices
        if (face.v0 >= vertexCount || face.v1 >= vertexCount || face.v2 >= vertexCount) {
            continue;
        }
        
        if (face.v0 == vertexID) {
            neighborSet.insert(face.v1);
            neighborSet.insert(face.v2);
        } else if (face.v1 == vertexID) {
            neighborSet.insert(face.v0);
            neighborSet.insert(face.v2);
        } else if (face.v2 == vertexID) {
            neighborSet.insert(face.v0);
            neighborSet.insert(face.v1);
        }
    }
    
    neighbors.assign(neighborSet.begin(), neighborSet.end());
    return neighbors;
}

std::vector<uint32_t> MeshSmooth::GetVertexNeighbors(const render::Mesh& mesh, uint32_t vertexID) {
    std::vector<uint32_t> neighbors;
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
    
    if (indices.empty() || vertexID >= vertices.size()) {
        return neighbors;
    }
    
    // Build topology and use the optimized version
    MeshTopology topology;
    topology.BuildFromIndices(vertices.size(), indices);
    if (!topology.IsValid()) return neighbors;
    
    return GetVertexNeighbors(topology, vertexID, vertices.size());
}

math::Vec3 MeshSmooth::ComputeLaplacian(const render::Mesh& mesh, uint32_t vertexID,
                                        const std::vector<uint32_t>& neighbors) {
    if (neighbors.empty()) {
        return math::Vec3(0.0f);
    }
    
    const auto& vertices = mesh.GetVertices();
    if (vertexID >= vertices.size()) {
        return math::Vec3(0.0f);
    }
    
    math::Vec3 currentPos = vertices[vertexID].position;
    math::Vec3 laplacian(0.0f);
    
    for (uint32_t nid : neighbors) {
        if (nid < vertices.size()) {
            laplacian += vertices[nid].position - currentPos;
        }
    }
    
    return laplacian / static_cast<float>(neighbors.size());
}

} // namespace geometry
} // namespace eyelib
