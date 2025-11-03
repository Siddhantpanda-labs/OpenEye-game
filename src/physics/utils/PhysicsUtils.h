#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <btBulletDynamicsCommon.h>

namespace OpenEye {
namespace Physics {
namespace Utils {

/**
 * @brief Utility functions for GLM/Bullet conversions.
 */
class PhysicsUtils {
public:
    // GLM to Bullet conversions
    static btVector3 GlmToBullet(const glm::vec3& v);
    static btQuaternion GlmToBullet(const glm::quat& q);
    
    // Bullet to GLM conversions
    static glm::vec3 BulletToGlm(const btVector3& v);
    static glm::quat BulletToGlm(const btQuaternion& q);
};

} // namespace Utils
} // namespace Physics
} // namespace OpenEye

