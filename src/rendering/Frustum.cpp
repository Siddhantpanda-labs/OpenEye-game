#include "Frustum.h"
#include <algorithm>
#include <cmath>

namespace OpenEye {
namespace Rendering {

void Frustum::ExtractFromMatrix(const glm::mat4& viewProj) {
    // Extract frustum planes from combined view-projection matrix
    // Left plane
    m_planes[0].x = viewProj[0][3] + viewProj[0][0];
    m_planes[0].y = viewProj[1][3] + viewProj[1][0];
    m_planes[0].z = viewProj[2][3] + viewProj[2][0];
    m_planes[0].w = viewProj[3][3] + viewProj[3][0];
    
    // Right plane
    m_planes[1].x = viewProj[0][3] - viewProj[0][0];
    m_planes[1].y = viewProj[1][3] - viewProj[1][0];
    m_planes[1].z = viewProj[2][3] - viewProj[2][0];
    m_planes[1].w = viewProj[3][3] - viewProj[3][0];
    
    // Bottom plane
    m_planes[2].x = viewProj[0][3] + viewProj[0][1];
    m_planes[2].y = viewProj[1][3] + viewProj[1][1];
    m_planes[2].z = viewProj[2][3] + viewProj[2][1];
    m_planes[2].w = viewProj[3][3] + viewProj[3][1];
    
    // Top plane
    m_planes[3].x = viewProj[0][3] - viewProj[0][1];
    m_planes[3].y = viewProj[1][3] - viewProj[1][1];
    m_planes[3].z = viewProj[2][3] - viewProj[2][1];
    m_planes[3].w = viewProj[3][3] - viewProj[3][1];
    
    // Near plane
    m_planes[4].x = viewProj[0][3] + viewProj[0][2];
    m_planes[4].y = viewProj[1][3] + viewProj[1][2];
    m_planes[4].z = viewProj[2][3] + viewProj[2][2];
    m_planes[4].w = viewProj[3][3] + viewProj[3][2];
    
    // Far plane
    m_planes[5].x = viewProj[0][3] - viewProj[0][2];
    m_planes[5].y = viewProj[1][3] - viewProj[1][2];
    m_planes[5].z = viewProj[2][3] - viewProj[2][2];
    m_planes[5].w = viewProj[3][3] - viewProj[3][2];
    
    // Normalize all planes
    for (auto& plane : m_planes) {
        NormalizePlane(plane);
    }
}

void Frustum::NormalizePlane(glm::vec4& plane) {
    float length = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
    if (length > 0.0f) {
        plane.x /= length;
        plane.y /= length;
        plane.z /= length;
        plane.w /= length;
    }
}

int Frustum::TestAABB(const glm::vec3& min, const glm::vec3& max) const {
    int result = 2; // Assume inside
    
    // Test against all 6 planes
    for (const auto& plane : m_planes) {
        // Find the farthest point from the plane in the direction of the plane normal
        glm::vec3 positiveVertex(
            plane.x > 0.0f ? max.x : min.x,
            plane.y > 0.0f ? max.y : min.y,
            plane.z > 0.0f ? max.z : min.z
        );
        
        // Calculate distance from farthest point to plane
        float distance = glm::dot(glm::vec3(plane), positiveVertex) + plane.w;
        
        if (distance < 0.0f) {
            // AABB is completely outside this plane
            return 0;
        }
        
        // Check if any point is outside (if so, we intersect)
        glm::vec3 negativeVertex(
            plane.x < 0.0f ? max.x : min.x,
            plane.y < 0.0f ? max.y : min.y,
            plane.z < 0.0f ? max.z : min.z
        );
        
        float negativeDistance = glm::dot(glm::vec3(plane), negativeVertex) + plane.w;
        
        if (negativeDistance < 0.0f) {
            result = 1; // Intersecting
        }
    }
    
    return result;
}

bool Frustum::TestPoint(const glm::vec3& point) const {
    for (const auto& plane : m_planes) {
        float distance = glm::dot(glm::vec3(plane), point) + plane.w;
        if (distance < 0.0f) {
            return false; // Point is outside
        }
    }
    return true;
}

bool Frustum::TestSphere(const glm::vec3& center, float radius) const {
    for (const auto& plane : m_planes) {
        float distance = glm::dot(glm::vec3(plane), center) + plane.w;
        if (distance < -radius) {
            return false; // Sphere is completely outside
        }
    }
    return true;
}

} // namespace Rendering
} // namespace OpenEye

