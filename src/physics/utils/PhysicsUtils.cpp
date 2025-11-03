#include "PhysicsUtils.h"

namespace OpenEye {
namespace Physics {
namespace Utils {

btVector3 PhysicsUtils::GlmToBullet(const glm::vec3& v) {
    return btVector3(v.x, v.y, v.z);
}

btQuaternion PhysicsUtils::GlmToBullet(const glm::quat& q) {
    return btQuaternion(q.x, q.y, q.z, q.w);
}

glm::vec3 PhysicsUtils::BulletToGlm(const btVector3& v) {
    return glm::vec3(v.x(), v.y(), v.z());
}

glm::quat PhysicsUtils::BulletToGlm(const btQuaternion& q) {
    return glm::quat(q.w(), q.x(), q.y(), q.z());
}

} // namespace Utils
} // namespace Physics
} // namespace OpenEye

