#include "eyelib/geometry/operations/MeshDisplace.h"
#include <algorithm>
#include <cmath>

namespace eyelib {
namespace geometry {

void MeshDisplace::DisplaceAlongNormal(render::Mesh& mesh, const Selection& selection,
                                       float amount, const Mask* mask) {
    auto& vertices = mesh.GetVertices();
    if (vertices.empty()) return;
    
    // Determine which vertices to displace
    std::unordered_set<uint32_t> verticesToDisplace;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToDisplace.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToDisplace.insert(vid);
            }
        }
    }
    
    // Displace vertices
    for (uint32_t vid : verticesToDisplace) {
        if (vid >= vertices.size()) continue;
        
        auto& vert = vertices[vid];
        math::Vec3 normal = glm::normalize(vert.normal);
        
        float finalAmount = amount;
        if (mask) {
            finalAmount *= mask->GetWeight(vid);
        }
        
        vert.position += normal * finalAmount;
    }
    
    // Recalculate normals and update GPU
    mesh.GenerateNormals();
    mesh.InvalidateAABB();
    mesh.UploadToGPU();
}

void MeshDisplace::DisplaceAlongDirection(render::Mesh& mesh, const Selection& selection,
                                          const math::Vec3& direction, float amount,
                                          const Mask* mask) {
    auto& vertices = mesh.GetVertices();
    if (vertices.empty()) return;
    
    math::Vec3 normalizedDir = glm::normalize(direction);
    
    // Determine which vertices to displace
    std::unordered_set<uint32_t> verticesToDisplace;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToDisplace.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToDisplace.insert(vid);
            }
        }
    }
    
    // Displace vertices
    for (uint32_t vid : verticesToDisplace) {
        if (vid >= vertices.size()) continue;
        
        float finalAmount = amount;
        if (mask) {
            finalAmount *= mask->GetWeight(vid);
        }
        
        vertices[vid].position += normalizedDir * finalAmount;
    }
    
    // Recalculate normals and update GPU
    mesh.GenerateNormals();
    mesh.InvalidateAABB();
    mesh.UploadToGPU();
}

void MeshDisplace::DisplaceWithField(render::Mesh& mesh, const Selection& selection,
                                     std::function<float(const math::Vec3&)> field,
                                     const math::Vec3* direction, const Mask* mask) {
    auto& vertices = mesh.GetVertices();
    if (vertices.empty()) return;
    
    // Determine which vertices to displace
    std::unordered_set<uint32_t> verticesToDisplace;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToDisplace.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToDisplace.insert(vid);
            }
        }
    }
    
    // Displace vertices
    for (uint32_t vid : verticesToDisplace) {
        if (vid >= vertices.size()) continue;
        
        auto& vert = vertices[vid];
        float fieldValue = field(vert.position);
        
        math::Vec3 dispDir;
        if (direction) {
            dispDir = glm::normalize(*direction);
        } else {
            dispDir = glm::normalize(vert.normal);
        }
        
        float finalAmount = fieldValue;
        if (mask) {
            finalAmount *= mask->GetWeight(vid);
        }
        
        vert.position += dispDir * finalAmount;
    }
    
    // Recalculate normals and update GPU
    mesh.GenerateNormals();
    mesh.InvalidateAABB();
    mesh.UploadToGPU();
}

} // namespace geometry
} // namespace eyelib
