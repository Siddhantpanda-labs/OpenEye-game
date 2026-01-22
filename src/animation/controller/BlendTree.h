#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace OpenEye {
namespace Animation {
namespace Controller {

enum class BlendTreeType {
    Blend1D,
    Blend2D
};

struct BlendTreeChild {
    std::string motionClip;   // clip name (for now)
    float threshold = 0.0f;   // 1D
    glm::vec2 position{0.0f}; // 2D
};

struct BlendTree {
    std::string name;
    BlendTreeType type = BlendTreeType::Blend1D;

    // Parameter names to sample
    std::string paramX = "speed"; // 1D uses paramX
    std::string paramY = "strafe"; // 2D uses both

    std::vector<BlendTreeChild> children;

    // Evaluate weights given input (returns weights parallel to children; sums to 1 when possible)
    std::vector<float> Evaluate1D(float x) const;
    std::vector<float> Evaluate2D(const glm::vec2& xy) const;
};

} // namespace Controller
} // namespace Animation
} // namespace OpenEye

