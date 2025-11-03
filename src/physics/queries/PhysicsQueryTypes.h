#pragma once

#include <glm/glm.hpp>

namespace OpenEye {
namespace Physics {

/**
 * @brief Result structures for physics queries.
 */
struct RaycastResult {
    bool hit = false;
    glm::vec3 point;
    glm::vec3 normal;
    void* userData = nullptr;  // Can store Entity pointer
};

struct SweepResult {
    bool hit = false;
    float hitFraction = 1.0f;
    glm::vec3 hitPoint;
    glm::vec3 hitNormal;
    void* userData = nullptr;
};

} // namespace Physics
} // namespace OpenEye

