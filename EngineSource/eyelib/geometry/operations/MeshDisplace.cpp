#include "eyelib/geometry/operations/MeshDisplace.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/MeshDescriptorUtils.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace eyelib {
namespace geometry {

void MeshDisplace::DisplaceAlongNormal(geometry::EditableMesh& editable, const Selection& selection,
                                       float amount, const Mask* mask) {
    auto& vertices = editable.GetVertices();
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
    
    auto desc = editable.GetDescriptor();
    render::MeshDescriptorUtils::GenerateNormals(desc);
    editable.SetVertices(std::move(desc.vertices));
}

void MeshDisplace::DisplaceAlongDirection(geometry::EditableMesh& editable, const Selection& selection,
                                          const math::Vec3& direction, float amount,
                                          const Mask* mask) {
    auto& vertices = editable.GetVertices();
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
    
    auto desc = editable.GetDescriptor();
    render::MeshDescriptorUtils::GenerateNormals(desc);
    editable.SetVertices(std::move(desc.vertices));
}

void MeshDisplace::DisplaceWithField(geometry::EditableMesh& editable, const Selection& selection,
                                     std::function<float(const math::Vec3&)> field,
                                     const math::Vec3* direction, const Mask* mask) {
    auto& vertices = editable.GetVertices();
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
    
    auto desc = editable.GetDescriptor();
    render::MeshDescriptorUtils::GenerateNormals(desc);
    editable.SetVertices(std::move(desc.vertices));
}

} // namespace geometry
} // namespace eyelib
