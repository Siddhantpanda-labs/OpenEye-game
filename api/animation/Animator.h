#pragma once

#include "AnimationClip.h"
#include "BlendTree.h"
#include "Skeleton.h"
#include "StateMachine.h"

#include "eyelib/math/Types.h"

#include <memory>
#include <vector>

namespace eyelib {
namespace animation { class Animator; }
namespace api {

class Animator {
public:
    Animator();
    ~Animator();

    Animator(const Animator&) = delete;
    Animator& operator=(const Animator&) = delete;

    void SetSkeleton(std::shared_ptr<Skeleton> skeleton);
    void SetClip(std::shared_ptr<AnimationClip> clip);
    void SetStateMachine(std::shared_ptr<StateMachine> sm);
    void SetBlendTree(std::shared_ptr<BlendTree> tree);

    void Update(float deltaTime);
    const std::vector<eyelib::math::Mat4>& GetBoneMatrices() const;

    void SetParameter(const std::string& name, float value);
    void SetParameter(const std::string& name, bool value);
    void SetTrigger(const std::string& name);

    static std::shared_ptr<Animator> WrapInternal(const std::shared_ptr<eyelib::animation::Animator>& animator);
    std::shared_ptr<eyelib::animation::Animator> GetInternal() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
