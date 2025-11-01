#pragma once

#include <glm/glm.hpp>
#include <array>

namespace OpenEye {
namespace Rendering {

// Represents a view frustum with 6 planes (left, right, bottom, top, near, far)
class Frustum {
public:
    enum class Plane : int {
        Left = 0,
        Right = 1,
        Bottom = 2,
        Top = 3,
        Near = 4,
        Far = 5
    };
    
    Frustum() = default;
    
    // Extract frustum planes from view-projection matrix
    void ExtractFromMatrix(const glm::mat4& viewProj);
    
    // Test if an AABB intersects or is inside the frustum
    // Returns: 0 = outside, 1 = intersect, 2 = inside
    int TestAABB(const glm::vec3& min, const glm::vec3& max) const;
    
    // Test if a point is inside the frustum
    bool TestPoint(const glm::vec3& point) const;
    
    // Test if a sphere intersects the frustum
    bool TestSphere(const glm::vec3& center, float radius) const;
    
    // Get plane by index
    const glm::vec4& GetPlane(Plane plane) const { return m_planes[static_cast<int>(plane)]; }

private:
    std::array<glm::vec4, 6> m_planes; // ax + by + cz + d = 0 format
    
    void NormalizePlane(glm::vec4& plane);
};

} // namespace Rendering
} // namespace OpenEye

