#pragma once

#include "AnimationClip.h"
#include "BlendSystem.h"
#include "Pose.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace animation {

enum class BlendTreeType {
    Blend1D,
    Blend2D
};

struct BlendTreeChild {
    std::shared_ptr<AnimationClip> clip;
    float threshold = 0.0f;      // 1D
    math::Vec2 position{0.0f};   // 2D
};

class BlendTree {
public:
    void SetName(const std::string& name) { m_name = name; }
    const std::string& Name() const { return m_name; }

    void SetType(BlendTreeType type) { m_type = type; }
    BlendTreeType Type() const { return m_type; }

    void SetParams(const std::string& xParam, const std::string& yParam = "") {
        m_paramX = xParam;
        m_paramY = yParam;
    }

    void AddChild(const BlendTreeChild& child) { m_children.push_back(child); }

    std::vector<float> Evaluate1D(float x) const;
    std::vector<float> Evaluate2D(const math::Vec2& xy) const;

    Pose Evaluate(const std::unordered_map<std::string, float>& params, float time, size_t boneCount) const;

private:
    std::string m_name{};
    BlendTreeType m_type = BlendTreeType::Blend1D;
    std::string m_paramX{"speed"};
    std::string m_paramY{"strafe"};
    std::vector<BlendTreeChild> m_children;
};

} // namespace animation
} // namespace eyelib
