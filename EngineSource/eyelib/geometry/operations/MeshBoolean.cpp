#include "eyelib/geometry/operations/MeshBoolean.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/MeshDescriptorUtils.h"
#include "eyelib/math/Intersection.h"
#include "eyelib/math/Ray.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <unordered_set>

namespace eyelib {
namespace geometry {

void MeshBoolean::Union(geometry::EditableMesh& meshA, const geometry::EditableMesh& meshB) {
    // Union: Keep all faces from A that are outside B, and all faces from B that are outside A
    ClassifyAndMerge(meshA, meshA, meshB, false, true, false, true);
}

void MeshBoolean::Difference(geometry::EditableMesh& meshA, const geometry::EditableMesh& meshB) {
    // Difference: Keep faces from A that are outside B, remove faces from A that are inside B
    ClassifyAndMerge(meshA, meshA, meshB, false, true, false, false);
}

void MeshBoolean::Intersection(geometry::EditableMesh& meshA, const geometry::EditableMesh& meshB) {
    // Intersection: Keep only faces that are inside both meshes
    ClassifyAndMerge(meshA, meshA, meshB, true, false, true, false);
}

bool MeshBoolean::IsVertexInside(const geometry::EditableMesh& editable, const math::Vec3& point) {
    // Use ray casting to determine if point is inside mesh
    // Cast ray in random direction and count intersections
    math::Vec3 direction(1.0f, 0.0f, 0.0f); // Simple direction
    return RayCastInsideTest(editable, point, direction);
}

math::AABB MeshBoolean::ComputeMeshBounds(const geometry::EditableMesh& editable) {
    const auto& vertices = editable.GetVertices();
    if (vertices.empty()) {
        return math::AABB();
    }
    
    math::Vec3 min = vertices[0].position;
    math::Vec3 max = vertices[0].position;
    
    for (const auto& vertex : vertices) {
        min = glm::min(min, vertex.position);
        max = glm::max(max, vertex.position);
    }
    
    return math::AABB(min, max);
}

bool MeshBoolean::RayCastInsideTest(const geometry::EditableMesh& editable, const math::Vec3& point, const math::Vec3& direction) {
    // Cast ray from point and count triangle intersections
    // Odd number = inside, even number = outside
    
    math::Ray ray(point, direction);
    
    // Get indices
    const auto& indices = editable.GetIndices();
    const auto& vertices = editable.GetVertices();
    int intersectionCount = 0;
    
    // Test against all triangles
    for (size_t i = 0; i < indices.size(); i += 3) {
        if (i + 2 >= indices.size()) break;
        
        uint32_t v0 = indices[i];
        uint32_t v1 = indices[i + 1];
        uint32_t v2 = indices[i + 2];
        
        if (v0 >= vertices.size() || v1 >= vertices.size() || v2 >= vertices.size()) continue;
        
        math::Vec3 p0 = vertices[v0].position;
        math::Vec3 p1 = vertices[v1].position;
        math::Vec3 p2 = vertices[v2].position;
        
        math::RayHit hit = math::RayTriangle(ray, p0, p1, p2);
        if (hit.hit && hit.t > 0.001f) { // Small epsilon to avoid self-intersection
            intersectionCount++;
        }
    }
    
    return (intersectionCount % 2) == 1; // Odd = inside
}

void MeshBoolean::ClassifyAndMerge(geometry::EditableMesh& resultMesh, const geometry::EditableMesh& meshA, const geometry::EditableMesh& meshB,
                                  bool keepAInside, bool keepAOutside, bool keepBInside, bool keepBOutside) {
    const auto& verticesA = meshA.GetVertices();
    const auto& verticesB = meshB.GetVertices();
    
    if (verticesA.empty() && verticesB.empty()) return;
    
    // Get indices
    const auto& indicesA = meshA.GetIndices();
    const auto& indicesB = meshB.GetIndices();
    
    // Build new mesh
    std::vector<render::Vertex> newVertices;
    std::vector<uint32_t> newIndices;
    std::unordered_map<uint32_t, uint32_t> vertexMapA; // Old index -> new index
    std::unordered_map<uint32_t, uint32_t> vertexMapB;
    
    // Process mesh A faces
    for (size_t i = 0; i < indicesA.size(); i += 3) {
        if (i + 2 >= indicesA.size()) break;
        
        uint32_t v0 = indicesA[i];
        uint32_t v1 = indicesA[i + 1];
        uint32_t v2 = indicesA[i + 2];
        
        if (v0 >= verticesA.size() || v1 >= verticesA.size() || v2 >= verticesA.size()) continue;
        
        // Compute face center for classification
        math::Vec3 faceCenter = (verticesA[v0].position + verticesA[v1].position + verticesA[v2].position) / 3.0f;
        bool isInsideB = IsVertexInside(meshB, faceCenter);
        
        bool shouldKeep = (isInsideB && keepAInside) || (!isInsideB && keepAOutside);
        
        if (shouldKeep) {
            // Add vertices if not already added
            auto getOrAddVertex = [&](uint32_t oldVid, std::unordered_map<uint32_t, uint32_t>& vertexMap) -> uint32_t {
                auto it = vertexMap.find(oldVid);
                if (it != vertexMap.end()) {
                    return it->second;
                }
                uint32_t newVid = static_cast<uint32_t>(newVertices.size());
                newVertices.push_back(verticesA[oldVid]);
                vertexMap[oldVid] = newVid;
                return newVid;
            };
            
            uint32_t nv0 = getOrAddVertex(v0, vertexMapA);
            uint32_t nv1 = getOrAddVertex(v1, vertexMapA);
            uint32_t nv2 = getOrAddVertex(v2, vertexMapA);
            
            newIndices.push_back(nv0);
            newIndices.push_back(nv1);
            newIndices.push_back(nv2);
        }
    }
    
    // Process mesh B faces
    for (size_t i = 0; i < indicesB.size(); i += 3) {
        if (i + 2 >= indicesB.size()) break;
        
        uint32_t v0 = indicesB[i];
        uint32_t v1 = indicesB[i + 1];
        uint32_t v2 = indicesB[i + 2];
        
        if (v0 >= verticesB.size() || v1 >= verticesB.size() || v2 >= verticesB.size()) continue;
        
        // Compute face center for classification
        math::Vec3 faceCenter = (verticesB[v0].position + verticesB[v1].position + verticesB[v2].position) / 3.0f;
        bool isInsideA = IsVertexInside(meshA, faceCenter);
        
        bool shouldKeep = (isInsideA && keepBInside) || (!isInsideA && keepBOutside);
        
        if (shouldKeep) {
            // Add vertices if not already added
            auto getOrAddVertex = [&](uint32_t oldVid, std::unordered_map<uint32_t, uint32_t>& vertexMap) -> uint32_t {
                auto it = vertexMap.find(oldVid);
                if (it != vertexMap.end()) {
                    return it->second;
                }
                uint32_t newVid = static_cast<uint32_t>(newVertices.size());
                newVertices.push_back(verticesB[oldVid]);
                vertexMap[oldVid] = newVid;
                return newVid;
            };
            
            uint32_t nv0 = getOrAddVertex(v0, vertexMapB);
            uint32_t nv1 = getOrAddVertex(v1, vertexMapB);
            uint32_t nv2 = getOrAddVertex(v2, vertexMapB);
            
            newIndices.push_back(nv0);
            newIndices.push_back(nv1);
            newIndices.push_back(nv2);
        }
    }
    
    // Update result mesh
    resultMesh.SetVertices(std::move(newVertices));
    resultMesh.SetIndices(std::move(newIndices));
    auto desc = resultMesh.GetDescriptor();
    render::MeshDescriptorUtils::GenerateNormals(desc);
    render::MeshDescriptorUtils::GenerateTangents(desc);
    resultMesh.SetVertices(std::move(desc.vertices));
}

} // namespace geometry
} // namespace eyelib
