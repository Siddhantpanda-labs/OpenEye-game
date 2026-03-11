#include "eyelib/geometry/operations/MeshDeform.h"
#include "eyelib/geometry/core/MeshTopology.h"
#include "eyelib/geometry/operations/MeshSmooth.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/MeshDescriptorUtils.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <unordered_set>
#include <cmath>

namespace eyelib {
namespace geometry {

void MeshDeform::Inflate(geometry::EditableMesh& editable, const Selection& selection,
                         float amount, const Mask* mask) {
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    // Build topology for normal computation
    MeshTopology topology;
    std::vector<uint32_t> indices;
    indices = editable.GetIndices();
    
    if (indices.empty()) return;
    topology.BuildFromIndices(vertices.size(), indices);
    if (!topology.IsValid()) {
        // Fallback: use vertex normals directly
        for (auto& vert : vertices) {
            math::Vec3 normal = glm::normalize(vert.normal);
            if (glm::length(normal) < 0.001f) continue;
            
            float finalAmount = amount;
            if (mask) {
                finalAmount *= mask->GetWeight(static_cast<uint32_t>(&vert - &vertices[0]));
            }
            vert.position += normal * finalAmount;
        }
    } else {
        // Determine which vertices to inflate
        std::unordered_set<uint32_t> verticesToInflate;
        if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
            for (size_t i = 0; i < vertices.size(); ++i) {
                verticesToInflate.insert(static_cast<uint32_t>(i));
            }
        } else {
            for (uint32_t vid : selection.GetIDs()) {
                if (vid < vertices.size()) {
                    verticesToInflate.insert(vid);
                }
            }
        }
        
        // Inflate vertices along normals
        for (uint32_t vid : verticesToInflate) {
            if (vid >= vertices.size()) continue;
            
            math::Vec3 normal = ComputeVertexNormal(editable, vid, topology, vertices.size());
            if (glm::length(normal) < 0.001f) continue;
            
            float finalAmount = amount;
            if (mask) {
                finalAmount *= mask->GetWeight(vid);
            }
            
            vertices[vid].position += normal * finalAmount;
        }
    }
    
    // Recalculate normals and update GPU
    auto desc = editable.GetDescriptor();
    render::MeshDescriptorUtils::GenerateNormals(desc);
    editable.SetVertices(std::move(desc.vertices));
}

void MeshDeform::Deflate(geometry::EditableMesh& editable, const Selection& selection,
                        float amount, const Mask* mask) {
    // Deflate is just negative inflate
    Inflate(editable, selection, -amount, mask);
}

void MeshDeform::Relax(geometry::EditableMesh& editable, const Selection& selection,
                      float strength, int iterations, const Mask* mask) {
    // Relax is gentle smoothing - use SmoothUniform with lower strength
    // Typically relax uses strength around 0.1-0.2 vs normal smooth at 0.5+
    MeshSmooth::SmoothUniform(editable, selection, strength, iterations, mask);
}

void MeshDeform::Lattice(geometry::EditableMesh& editable, const Selection& selection,
                        const math::Vec3& latticeOrigin, const math::Vec3& latticeSize,
                        const math::IVec3& latticeResolution,
                        const std::vector<math::Vec3>& controlPoints,
                        const Mask* mask) {
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    // Validate control points
    size_t expectedPoints = static_cast<size_t>(latticeResolution.x) * 
                           static_cast<size_t>(latticeResolution.y) * 
                           static_cast<size_t>(latticeResolution.z);
    if (controlPoints.size() != expectedPoints) {
        // Invalid control points, skip
        return;
    }
    
    // Determine which vertices to deform
    std::unordered_set<uint32_t> verticesToDeform;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToDeform.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToDeform.insert(vid);
            }
        }
    }
    
    // Deform vertices using lattice
    for (uint32_t vid : verticesToDeform) {
        if (vid >= vertices.size()) continue;
        
        math::Vec3 pos = vertices[vid].position;
        
        // Compute local position in lattice space
        math::Vec3 localPos = pos - latticeOrigin;
        math::Vec3 normalizedPos = localPos / latticeSize;
        
        // Clamp to lattice bounds
        normalizedPos.x = std::clamp(normalizedPos.x, 0.0f, 1.0f);
        normalizedPos.y = std::clamp(normalizedPos.y, 0.0f, 1.0f);
        normalizedPos.z = std::clamp(normalizedPos.z, 0.0f, 1.0f);
        
        // Compute displacement from lattice
        math::Vec3 displacement(0.0f);
        float totalWeight = 0.0f;
        
        // Use trilinear interpolation from surrounding control points
        float fx = normalizedPos.x * (latticeResolution.x - 1);
        float fy = normalizedPos.y * (latticeResolution.y - 1);
        float fz = normalizedPos.z * (latticeResolution.z - 1);
        
        int x0 = static_cast<int>(std::floor(fx));
        int y0 = static_cast<int>(std::floor(fy));
        int z0 = static_cast<int>(std::floor(fz));
        
        int x1 = std::min(x0 + 1, latticeResolution.x - 1);
        int y1 = std::min(y0 + 1, latticeResolution.y - 1);
        int z1 = std::min(z0 + 1, latticeResolution.z - 1);
        
        float dx = fx - x0;
        float dy = fy - y0;
        float dz = fz - z0;
        
        // Trilinear interpolation
        auto getControlPoint = [&](int x, int y, int z) -> math::Vec3 {
            int idx = z * (latticeResolution.x * latticeResolution.y) + 
                     y * latticeResolution.x + x;
            if (idx >= 0 && idx < static_cast<int>(controlPoints.size())) {
                return controlPoints[idx];
            }
            return math::Vec3(0.0f);
        };
        
        math::Vec3 c000 = getControlPoint(x0, y0, z0);
        math::Vec3 c100 = getControlPoint(x1, y0, z0);
        math::Vec3 c010 = getControlPoint(x0, y1, z0);
        math::Vec3 c110 = getControlPoint(x1, y1, z0);
        math::Vec3 c001 = getControlPoint(x0, y0, z1);
        math::Vec3 c101 = getControlPoint(x1, y0, z1);
        math::Vec3 c011 = getControlPoint(x0, y1, z1);
        math::Vec3 c111 = getControlPoint(x1, y1, z1);
        
        // Interpolate
        math::Vec3 c00 = c000 * (1.0f - dx) + c100 * dx;
        math::Vec3 c01 = c001 * (1.0f - dx) + c101 * dx;
        math::Vec3 c10 = c010 * (1.0f - dx) + c110 * dx;
        math::Vec3 c11 = c011 * (1.0f - dx) + c111 * dx;
        
        math::Vec3 c0 = c00 * (1.0f - dy) + c10 * dy;
        math::Vec3 c1 = c01 * (1.0f - dy) + c11 * dy;
        
        displacement = c0 * (1.0f - dz) + c1 * dz;
        
        // Apply mask if provided
        float maskWeight = 1.0f;
        if (mask) {
            maskWeight = mask->GetWeight(vid);
        }
        
        // Apply displacement
        vertices[vid].position += displacement * maskWeight;
    }
    
    // Recalculate normals and update GPU
    auto desc = editable.GetDescriptor();
    render::MeshDescriptorUtils::GenerateNormals(desc);
    editable.SetVertices(std::move(desc.vertices));
}

math::Vec3 MeshDeform::ComputeVertexNormal(const geometry::EditableMesh& editable, uint32_t vertexID,
                                          const MeshTopology& topology, size_t vertexCount) {
    const auto& vertices = editable.GetVertices();
    if (vertexID >= vertices.size()) return math::Vec3(0, 1, 0);
    
    // Use stored normal if valid
    math::Vec3 normal = glm::normalize(vertices[vertexID].normal);
    if (glm::length(normal) > 0.001f) {
        return normal;
    }
    
    // Compute from faces if normal is invalid
    const auto& faces = topology.GetVertexFaces(vertexID);
    math::Vec3 computedNormal(0.0f);
    
    for (uint32_t faceIdx : faces) {
        if (faceIdx >= topology.GetFaceCount()) continue;
        
        const auto& face = topology.GetFace(faceIdx);
        if (face.v0 >= vertexCount || face.v1 >= vertexCount || face.v2 >= vertexCount) continue;
        
        math::Vec3 p0 = vertices[face.v0].position;
        math::Vec3 p1 = vertices[face.v1].position;
        math::Vec3 p2 = vertices[face.v2].position;
        
        math::Vec3 edge1 = p1 - p0;
        math::Vec3 edge2 = p2 - p0;
        math::Vec3 faceNormal = glm::cross(edge1, edge2);
        
        float len = glm::length(faceNormal);
        if (len > 0.001f) {
            computedNormal += faceNormal / len;
        }
    }
    
    float len = glm::length(computedNormal);
    if (len > 0.001f) {
        return computedNormal / len;
    }
    
    return math::Vec3(0, 1, 0);
}

float MeshDeform::ComputeLatticeWeight(const math::Vec3& point, const math::Vec3& latticeOrigin,
                                      const math::Vec3& latticeSize, const math::IVec3& resolution,
                                      int x, int y, int z) {
    // Compute distance from point to control point
    math::Vec3 controlPos = latticeOrigin + math::Vec3(
        (x / static_cast<float>(resolution.x - 1)) * latticeSize.x,
        (y / static_cast<float>(resolution.y - 1)) * latticeSize.y,
        (z / static_cast<float>(resolution.z - 1)) * latticeSize.z
    );
    
    float dist = glm::length(point - controlPos);
    float maxDist = glm::length(latticeSize);
    
    // Simple distance-based weight (can be improved with better falloff)
    if (maxDist > 0.001f) {
        return 1.0f - std::min(dist / maxDist, 1.0f);
    }
    
    return 0.0f;
}

} // namespace geometry
} // namespace eyelib
