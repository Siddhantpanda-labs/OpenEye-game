#include "IK.h"

#include "eyelib/math/TransformUtils.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace eyelib {
namespace animation {

namespace {
math::Vec3 GetBoneWorldPos(const Skeleton& skeleton, const Pose& pose, int boneIndex) {
    math::Mat4 m(1.0f);
    int idx = boneIndex;
    while (idx >= 0) {
        const Bone& b = skeleton.GetBone(static_cast<size_t>(idx));
        const math::Transform& t = pose.GetBoneTransform(static_cast<size_t>(idx));
        m = math::Compose(t.position, t.rotation, t.scale) * m;
        idx = b.parentIndex;
    }
    return math::Vec3(m * math::Vec4(0, 0, 0, 1));
}

void ApplyRotationToBone(Pose& pose, int boneIndex, const math::Quat& q) {
    math::Transform& t = pose.GetBoneTransform(static_cast<size_t>(boneIndex));
    t.rotation = q * t.rotation;
}
} // namespace

void IK::SolveCCD(const Skeleton& skeleton, Pose& pose, const IKChain& chain,
                  const math::Vec3& target, int iterations) {
    if (chain.bones.size() < 2) return;
    for (int it = 0; it < iterations; ++it) {
        int endBone = chain.bones.back();
        math::Vec3 endPos = GetBoneWorldPos(skeleton, pose, endBone);
        if (glm::length(target - endPos) < 1e-3f) break;

        for (int i = static_cast<int>(chain.bones.size()) - 2; i >= 0; --i) {
            int boneIdx = chain.bones[i];
            math::Vec3 bonePos = GetBoneWorldPos(skeleton, pose, boneIdx);
            endPos = GetBoneWorldPos(skeleton, pose, endBone);

            math::Vec3 toEnd = glm::normalize(endPos - bonePos);
            math::Vec3 toTarget = glm::normalize(target - bonePos);
            if (glm::length(toEnd) < 1e-4f || glm::length(toTarget) < 1e-4f) continue;

            math::Quat rot = glm::rotation(toEnd, toTarget);
            ApplyRotationToBone(pose, boneIdx, rot);
        }
    }
}

void IK::SolveFABRIK(const Skeleton& skeleton, Pose& pose, const IKChain& chain,
                     const math::Vec3& target, int iterations) {
    const size_t n = chain.bones.size();
    if (n < 2) return;

    std::vector<math::Vec3> positions(n);
    for (size_t i = 0; i < n; ++i) {
        positions[i] = GetBoneWorldPos(skeleton, pose, chain.bones[i]);
    }

    float totalLen = 0.0f;
    for (float l : chain.lengths) totalLen += l;

    math::Vec3 rootPos = positions.front();
    if (glm::length(target - rootPos) > totalLen) {
        // unreachable: stretch towards target
        for (size_t i = 0; i + 1 < n; ++i) {
            float r = glm::length(target - positions[i]);
            float lambda = chain.lengths[i] / r;
            positions[i + 1] = (1 - lambda) * positions[i] + lambda * target;
        }
    } else {
        for (int it = 0; it < iterations; ++it) {
            // forward
            positions.back() = target;
            for (int i = static_cast<int>(n) - 2; i >= 0; --i) {
                float r = glm::length(positions[i + 1] - positions[i]);
                float lambda = chain.lengths[i] / r;
                positions[i] = (1 - lambda) * positions[i + 1] + lambda * positions[i];
            }
            // backward
            positions.front() = rootPos;
            for (size_t i = 0; i + 1 < n; ++i) {
                float r = glm::length(positions[i + 1] - positions[i]);
                float lambda = chain.lengths[i] / r;
                positions[i + 1] = (1 - lambda) * positions[i] + lambda * positions[i + 1];
            }
            if (glm::length(positions.back() - target) < 1e-3f) break;
        }
    }

    // update pose rotations to match new bone directions
    for (size_t i = 0; i + 1 < n; ++i) {
        int boneIdx = chain.bones[i];
        math::Vec3 dir = glm::normalize(positions[i + 1] - positions[i]);
        math::Vec3 forward = math::Vec3(0, 0, 1);
        math::Quat rot = glm::rotation(forward, dir);
        pose.GetBoneTransform(static_cast<size_t>(boneIdx)).rotation = rot;
        pose.GetBoneTransform(static_cast<size_t>(boneIdx)).position = positions[i];
    }
    pose.GetBoneTransform(static_cast<size_t>(chain.bones.back())).position = positions.back();
}

} // namespace animation
} // namespace eyelib
