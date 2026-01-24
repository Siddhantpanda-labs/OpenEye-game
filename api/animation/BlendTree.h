#pragma once

#include "AnimationClip.h"
#include "eyelib/animation/BlendTree.h"

#include <memory>
#include <string>

namespace eyelib {
namespace api {

class BlendTree {
public:
    BlendTree();
    ~BlendTree();

    BlendTree(const BlendTree&) = delete;
    BlendTree& operator=(const BlendTree&) = delete;

    void SetName(const std::string& name);
    void SetType(int type); // 0=Blend1D,1=Blend2D
    void SetParams(const std::string& xParam, const std::string& yParam = "");

    void AddChild(const std::shared_ptr<AnimationClip>& clip, float threshold);
    void AddChild(const std::shared_ptr<AnimationClip>& clip, const eyelib::math::Vec2& position);

    std::shared_ptr<eyelib::animation::BlendTree> GetInternal() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    friend class Animator;
};

} // namespace api
} // namespace eyelib
