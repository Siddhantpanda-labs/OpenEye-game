#pragma once

#include "Pose.h"
#include "eyelib/math/Types.h"

#include <memory>
#include <vector>

namespace eyelib {
namespace animation {

class BoneModifier {
public:
    virtual ~BoneModifier() = default;
    virtual void Apply(Pose& pose, float deltaTime) = 0;
};

class SpringModifier : public BoneModifier {
public:
    SpringModifier(int boneIndex, float stiffness, float damping);
    void Apply(Pose& pose, float deltaTime) override;

private:
    int m_boneIndex = -1;
    float m_stiffness = 1.0f;
    float m_damping = 0.2f;
    math::Vec3 m_velocity{0.0f};
};

class NoiseModifier : public BoneModifier {
public:
    NoiseModifier(int boneIndex, float amplitude, float frequency);
    void Apply(Pose& pose, float deltaTime) override;

private:
    int m_boneIndex = -1;
    float m_amplitude = 0.1f;
    float m_frequency = 1.0f;
    float m_time = 0.0f;
};

class LookAtModifier : public BoneModifier {
public:
    LookAtModifier(int boneIndex, const math::Vec3& target, float strength = 1.0f);
    void Apply(Pose& pose, float deltaTime) override;

private:
    int m_boneIndex = -1;
    math::Vec3 m_target{0.0f};
    float m_strength = 1.0f;
};

class ProceduralAnimator {
public:
    void AddModifier(std::shared_ptr<BoneModifier> mod) { m_modifiers.push_back(std::move(mod)); }
    void Update(Pose& pose, float deltaTime);

private:
    std::vector<std::shared_ptr<BoneModifier>> m_modifiers;
};

} // namespace animation
} // namespace eyelib
