#include "api/animation/Procedural.h"
#include "eyelib/animation/Procedural.h"

namespace eyelib {
namespace api {

class ProceduralAnimator::Impl {
public:
    eyelib::animation::ProceduralAnimator animator;
};

ProceduralAnimator::ProceduralAnimator() : m_impl(std::make_unique<Impl>()) {}

ProceduralAnimator::~ProceduralAnimator() = default;

void ProceduralAnimator::AddSpringModifier(int boneIndex, float stiffness, float damping) {
    auto mod = std::make_shared<eyelib::animation::SpringModifier>(boneIndex, stiffness, damping);
    m_impl->animator.AddModifier(mod);
}

void ProceduralAnimator::AddNoiseModifier(int boneIndex, float amplitude, float frequency) {
    auto mod = std::make_shared<eyelib::animation::NoiseModifier>(boneIndex, amplitude, frequency);
    m_impl->animator.AddModifier(mod);
}

void ProceduralAnimator::AddLookAtModifier(int boneIndex, const math::Vec3& target, float strength) {
    auto mod = std::make_shared<eyelib::animation::LookAtModifier>(boneIndex, target, strength);
    m_impl->animator.AddModifier(mod);
}

void ProceduralAnimator::Update(Pose& pose, float deltaTime) {
    m_impl->animator.Update(*pose.GetInternal(), deltaTime);
}

} // namespace api
} // namespace eyelib
