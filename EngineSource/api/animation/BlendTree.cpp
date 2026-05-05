#include "BlendTree.h"

namespace eyelib {
namespace api {

class BlendTree::Impl {
public:
    std::shared_ptr<eyelib::animation::BlendTree> tree = std::make_shared<eyelib::animation::BlendTree>();
};

BlendTree::BlendTree() : m_impl(std::make_unique<Impl>()) {}
BlendTree::~BlendTree() = default;

void BlendTree::SetName(const std::string& name) {
    m_impl->tree->SetName(name);
}

void BlendTree::SetType(int type) {
    if (type == 1) m_impl->tree->SetType(eyelib::animation::BlendTreeType::Blend2D);
    else m_impl->tree->SetType(eyelib::animation::BlendTreeType::Blend1D);
}

void BlendTree::SetParams(const std::string& xParam, const std::string& yParam) {
    m_impl->tree->SetParams(xParam, yParam);
}

void BlendTree::AddChild(const std::shared_ptr<AnimationClip>& clip, float threshold) {
    eyelib::animation::BlendTreeChild c;
    if (clip) c.clip = clip->GetInternal();
    c.threshold = threshold;
    m_impl->tree->AddChild(c);
}

void BlendTree::AddChild(const std::shared_ptr<AnimationClip>& clip, const eyelib::math::Vec2& position) {
    eyelib::animation::BlendTreeChild c;
    if (clip) c.clip = clip->GetInternal();
    c.position = position;
    m_impl->tree->AddChild(c);
}

std::shared_ptr<eyelib::animation::BlendTree> BlendTree::GetInternal() const {
    return m_impl->tree;
}

} // namespace api
} // namespace eyelib
