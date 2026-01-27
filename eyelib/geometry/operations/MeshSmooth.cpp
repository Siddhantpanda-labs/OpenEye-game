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
    // Use Taubin (HC) smoothing by default
    if (iterations <= 0 || strength <= 0.0f) return;
    
    auto& vertices = mesh.GetVertices();
    if (vertices.empty()) return;
    
    // Build topology for neighbor queries
    MeshTopology topology;
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
    
    topology.BuildFromIndices(vertices.size(), indices);
    if (!topology.IsValid()) return;
    
    // Taubin parameters (industry-tested defaults)
    // lambda: smoothing step (positive)
    // mu: shape restoration step (negative, typically mu = -0.53 * lambda)
    float lambda = strength * 0.5f;  // Scale strength to reasonable range
    float mu = -0.53f * lambda;      // Standard Taubin ratio
    
    SmoothTaubin(mesh, selection, lambda, mu, iterations, mask, topology);
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
    
    // Uniform Laplacian smoothing iterations
    for (int iter = 0; iter < iterations; ++iter) {
        std::vector<math::Vec3> newPositions(vertices.size());
        
        // Copy current positions
        for (size_t i = 0; i < vertices.size(); ++i) {
            newPositions[i] = vertices[i].position;
        }
        
        // Apply uniform Laplacian smoothing
        for (uint32_t vid : verticesToSmooth) {
            if (vid >= vertices.size()) continue;
            
            math::Vec3 currentPos = vertices[vid].position;
            
            // Compute uniform Laplacian (one-ring average)
            math::Vec3 laplacian = ComputeUniformLaplacian(mesh, vid, topology, vertices.size());
            
            // Apply strength
            float finalStrength = strength;
            if (mask) {
                finalStrength *= mask->GetWeight(vid);
            }
            
            // Update position: p₁ = p₀ + λ · L(p₀)
            newPositions[vid] = currentPos + laplacian * finalStrength;
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

void MeshSmooth::SmoothTaubin(render::Mesh& mesh, const Selection& selection,
                              float lambda, float mu, int iterations,
                              const Mask* mask, const MeshTopology& topology) {
    auto& vertices = mesh.GetVertices();
    if (vertices.empty()) return;
    
    // Determine which vertices to smooth
    std::unordered_set<uint32_t> verticesToSmooth;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToSmooth.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToSmooth.insert(vid);
            }
        }
    }
    
    // Taubin smoothing iterations
    for (int iter = 0; iter < iterations; ++iter) {
        // Store original positions at start of iteration (p₀)
        std::vector<math::Vec3> p0(vertices.size());
        for (size_t i = 0; i < vertices.size(); ++i) {
            p0[i] = vertices[i].position;
        }
        
        std::vector<math::Vec3> p1(vertices.size());
        
        // Pass 1: Smooth (Uniform Laplacian)
        // p₁ = p₀ + λ · L(p₀)
        // Compute Laplacian directly from p0 positions (not from mesh)
        for (uint32_t vid : verticesToSmooth) {
            if (vid >= vertices.size()) continue;
            
            math::Vec3 currentPos = p0[vid];
            
            // Compute uniform Laplacian from p0 positions
            auto neighbors = GetVertexNeighbors(topology, vid, vertices.size());
            if (neighbors.empty()) {
                p1[vid] = currentPos;
                continue;
            }
            
            math::Vec3 neighborSum(0.0f);
            for (uint32_t nid : neighbors) {
                if (nid < vertices.size()) {
                    neighborSum += p0[nid];
                }
            }
            math::Vec3 averageNeighbor = neighborSum / static_cast<float>(neighbors.size());
            math::Vec3 laplacian = averageNeighbor - currentPos;
            
            // Apply lambda with mask
            float finalLambda = lambda;
            if (mask) {
                finalLambda *= mask->GetWeight(vid);
            }
            
            // Update position: p₁ = p₀ + λ · L(p₀)
            p1[vid] = currentPos + laplacian * finalLambda;
        }
        
        // Update mesh positions after pass 1
        for (size_t i = 0; i < vertices.size(); ++i) {
            vertices[i].position = p1[i];
        }
        
        // Pass 2: Inflate (Shape Restore)
        // b = p₁ − p₀, then p₂ = p₁ − μ · L(b)
        // First, compute displacement field b for all vertices
        std::vector<math::Vec3> displacementField(vertices.size());
        for (uint32_t vid : verticesToSmooth) {
            if (vid >= vertices.size()) continue;
            displacementField[vid] = p1[vid] - p0[vid];
        }
        
        // Now compute L(b) for each vertex and apply correction
        for (uint32_t vid : verticesToSmooth) {
            if (vid >= vertices.size()) continue;
            
            math::Vec3 p1Pos = p1[vid];  // Position after pass 1
            math::Vec3 b = displacementField[vid];   // Displacement at this vertex
            
            // Compute uniform Laplacian of displacement field: L(b)
            auto neighbors = GetVertexNeighbors(topology, vid, vertices.size());
            if (neighbors.empty()) {
                vertices[vid].position = p1Pos;
                continue;
            }
            
            math::Vec3 neighborDisplacementSum(0.0f);
            for (uint32_t nid : neighbors) {
                if (nid >= vertices.size()) continue;
                neighborDisplacementSum += displacementField[nid];
            }
            
            math::Vec3 averageNeighborDisplacement = neighborDisplacementSum / static_cast<float>(neighbors.size());
            math::Vec3 displacementLaplacian = averageNeighborDisplacement - b;
            
            // Apply mu with mask
            float finalMu = mu;
            if (mask) {
                finalMu *= mask->GetWeight(vid);
            }
            
            // Update position: p₂ = p₁ − μ · L(b)
            vertices[vid].position = p1Pos - displacementLaplacian * finalMu;
        }
    }
    
    // Recalculate normals and update GPU
    mesh.GenerateNormals();
    mesh.InvalidateAABB();
    mesh.UploadToGPU();
}

math::Vec3 MeshSmooth::ComputeUniformLaplacian(const render::Mesh& mesh, uint32_t vertexID,
                                                const MeshTopology& topology, size_t vertexCount) {
    const auto& vertices = mesh.GetVertices();
    if (vertexID >= vertices.size()) return math::Vec3(0.0f);
    
    math::Vec3 currentPos = vertices[vertexID].position;
    auto neighbors = GetVertexNeighbors(topology, vertexID, vertexCount);
    
    if (neighbors.empty()) return math::Vec3(0.0f);
    
    // Uniform Laplacian: L(p) = (1/N) Σ(p_j - p_i)
    // This is the one-ring average minus current position
    math::Vec3 neighborSum(0.0f);
    for (uint32_t nid : neighbors) {
        if (nid < vertices.size()) {
            neighborSum += vertices[nid].position;
        }
    }
    
    math::Vec3 averageNeighbor = neighborSum / static_cast<float>(neighbors.size());
    return averageNeighbor - currentPos;
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

} // namespace geometry
} // namespace eyelib
