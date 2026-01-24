#include "Pose.h"

#include <algorithm>

namespace eyelib {
namespace animation {

namespace {
math::Transform LerpTransform(const math::Transform& a, const math::Transform& b, float t) {
    math::Transform out;
    out.position = glm::mix(a.position, b.position, t);
    out.scale = glm::mix(a.scale, b.scale, t);
    out.rotation = glm::slerp(a.rotation, b.rotation, t);
    return out;
}
} // namespace

void Pose::SetBoneTransform(size_t index, const math::Transform& t) {
    if (index >= m_bones.size()) m_bones.resize(index + 1);
    m_bones[index] = t;
}

const math::Transform& Pose::GetBoneTransform(size_t index) const {
    return m_bones[index];
}

math::Transform& Pose::GetBoneTransform(size_t index) {
    return m_bones[index];
}

Pose Pose::Blend(const Pose& other, float weight) const {
    Pose out;
    const size_t count = std::max(m_bones.size(), other.m_bones.size());
    out.m_bones.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const math::Transform& a = (i < m_bones.size()) ? m_bones[i] : math::Transform{};
        const math::Transform& b = (i < other.m_bones.size()) ? other.m_bones[i] : math::Transform{};
        out.m_bones[i] = LerpTransform(a, b, weight);
    }
    return out;
}

void Pose::BlendInPlace(const Pose& other, float weight) {
    if (other.m_bones.size() > m_bones.size()) m_bones.resize(other.m_bones.size());
    for (size_t i = 0; i < m_bones.size(); ++i) {
        const math::Transform& b = (i < other.m_bones.size()) ? other.m_bones[i] : math::Transform{};
        m_bones[i] = LerpTransform(m_bones[i], b, weight);
    }
}

Pose Pose::ApplyAdditive(const Pose& additive, float weight) const {
    Pose out;
    const size_t count = std::max(m_bones.size(), additive.m_bones.size());
    out.m_bones.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const math::Transform& base = (i < m_bones.size()) ? m_bones[i] : math::Transform{};
        const math::Transform& add = (i < additive.m_bones.size()) ? additive.m_bones[i] : math::Transform{};
        math::Transform t = base;
        t.position += add.position * weight;
        t.scale += (add.scale - math::Vec3(1.0f)) * weight;
        t.rotation = glm::slerp(math::Quat(1.0f, 0.0f, 0.0f, 0.0f), add.rotation, weight) * base.rotation;
        out.m_bones[i] = t;
    }
    return out;
}

} // namespace animation
} // namespace eyelib
