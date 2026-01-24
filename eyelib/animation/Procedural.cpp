#include "Procedural.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace eyelib {
namespace animation {

SpringModifier::SpringModifier(int boneIndex, float stiffness, float damping)
    : m_boneIndex(boneIndex), m_stiffness(stiffness), m_damping(damping) {}

void SpringModifier::Apply(Pose& pose, float deltaTime) {
    if (m_boneIndex < 0 || static_cast<size_t>(m_boneIndex) >= pose.GetBoneCount()) return;
    math::Transform& t = pose.GetBoneTransform(static_cast<size_t>(m_boneIndex));
    math::Vec3 target = t.position;
    math::Vec3 force = (target - t.position) * m_stiffness - m_velocity * m_damping;
    m_velocity += force * deltaTime;
    t.position += m_velocity * deltaTime;
}

NoiseModifier::NoiseModifier(int boneIndex, float amplitude, float frequency)
    : m_boneIndex(boneIndex), m_amplitude(amplitude), m_frequency(frequency) {}

void NoiseModifier::Apply(Pose& pose, float deltaTime) {
    if (m_boneIndex < 0 || static_cast<size_t>(m_boneIndex) >= pose.GetBoneCount()) return;
    m_time += deltaTime;
    float n = std::sin(m_time * m_frequency);
    math::Transform& t = pose.GetBoneTransform(static_cast<size_t>(m_boneIndex));
    t.position += math::Vec3(n * m_amplitude);
}

LookAtModifier::LookAtModifier(int boneIndex, const math::Vec3& target, float strength)
    : m_boneIndex(boneIndex), m_target(target), m_strength(strength) {}

void LookAtModifier::Apply(Pose& pose, float deltaTime) {
    (void)deltaTime;
    if (m_boneIndex < 0 || static_cast<size_t>(m_boneIndex) >= pose.GetBoneCount()) return;
    math::Transform& t = pose.GetBoneTransform(static_cast<size_t>(m_boneIndex));
    math::Vec3 forward = math::Vec3(0, 0, 1);
    math::Vec3 dir = glm::normalize(m_target - t.position);
    if (glm::length(dir) < 1e-4f) return;
    math::Quat targetRot = glm::rotation(forward, dir);
    t.rotation = glm::slerp(t.rotation, targetRot, m_strength);
}

void ProceduralAnimator::Update(Pose& pose, float deltaTime) {
    for (auto& m : m_modifiers) {
        if (m) m->Apply(pose, deltaTime);
    }
}

} // namespace animation
} // namespace eyelib
