#pragma once

#include "api/animation/Pose.h"
#include "eyelib/math/Types.h"
#include <memory>

namespace eyelib {
namespace api {

class ProceduralAnimator {
public:
    ProceduralAnimator();
    ~ProceduralAnimator();

    ProceduralAnimator(const ProceduralAnimator&) = delete;
    ProceduralAnimator& operator=(const ProceduralAnimator&) = delete;

    void AddSpringModifier(int boneIndex, float stiffness, float damping);
    void AddNoiseModifier(int boneIndex, float amplitude, float frequency);
    void AddLookAtModifier(int boneIndex, const math::Vec3& target, float strength = 1.0f);

    void Update(Pose& pose, float deltaTime);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
