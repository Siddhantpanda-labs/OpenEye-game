#include "BlendTree.h"

#include <algorithm>
#include <limits>

namespace eyelib {
namespace animation {

std::vector<float> BlendTree::Evaluate1D(float x) const {
    std::vector<float> weights(m_children.size(), 0.0f);
    if (m_children.empty()) return weights;

    // ensure sorted by threshold
    std::vector<size_t> order(m_children.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return m_children[a].threshold < m_children[b].threshold;
    });

    if (x <= m_children[order.front()].threshold) {
        weights[order.front()] = 1.0f;
        return weights;
    }
    if (x >= m_children[order.back()].threshold) {
        weights[order.back()] = 1.0f;
        return weights;
    }

    for (size_t i = 0; i + 1 < order.size(); ++i) {
        const auto& c0 = m_children[order[i]];
        const auto& c1 = m_children[order[i + 1]];
        if (x >= c0.threshold && x <= c1.threshold) {
            float t = (c1.threshold - c0.threshold);
            t = (t > 0.0f) ? (x - c0.threshold) / t : 0.0f;
            weights[order[i]] = 1.0f - t;
            weights[order[i + 1]] = t;
            return weights;
        }
    }
    weights[order.back()] = 1.0f;
    return weights;
}

std::vector<float> BlendTree::Evaluate2D(const math::Vec2& xy) const {
    std::vector<float> weights(m_children.size(), 0.0f);
    if (m_children.empty()) return weights;

    float total = 0.0f;
    const float eps = 1e-4f;
    for (size_t i = 0; i < m_children.size(); ++i) {
        float d = glm::length(xy - m_children[i].position);
        if (d < eps) {
            weights[i] = 1.0f;
            return weights;
        }
        float w = 1.0f / d;
        weights[i] = w;
        total += w;
    }
    if (total > 0.0f) {
        for (auto& w : weights) w /= total;
    }
    return weights;
}

Pose BlendTree::Evaluate(const std::unordered_map<std::string, float>& params, float time, size_t boneCount) const {
    if (m_children.empty()) return Pose{};
    float x = 0.0f;
    float y = 0.0f;
    auto itX = params.find(m_paramX);
    if (itX != params.end()) x = itX->second;
    auto itY = params.find(m_paramY);
    if (itY != params.end()) y = itY->second;

    std::vector<float> weights = (m_type == BlendTreeType::Blend2D)
        ? Evaluate2D(math::Vec2(x, y))
        : Evaluate1D(x);

    std::vector<Pose> poses;
    poses.reserve(m_children.size());
    for (const auto& child : m_children) {
        if (child.clip) poses.push_back(BlendSystem::SampleClip(*child.clip, time, boneCount));
        else poses.push_back(Pose{});
    }
    return BlendSystem::Blend1D(poses, weights);
}

} // namespace animation
} // namespace eyelib
