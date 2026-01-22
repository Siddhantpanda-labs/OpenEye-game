#include "BlendTree.h"
#include <algorithm>
#include <cmath>

namespace OpenEye {
namespace Animation {
namespace Controller {

std::vector<float> BlendTree::Evaluate1D(float x) const {
    std::vector<float> w(children.size(), 0.0f);
    if (children.empty()) return w;
    if (children.size() == 1) { w[0] = 1.0f; return w; }

    // Sort indices by threshold
    std::vector<size_t> idx(children.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
        return children[a].threshold < children[b].threshold;
    });

    // Clamp to ends
    if (x <= children[idx.front()].threshold) { w[idx.front()] = 1.0f; return w; }
    if (x >= children[idx.back()].threshold) { w[idx.back()] = 1.0f; return w; }

    // Find segment
    for (size_t s = 0; s + 1 < idx.size(); ++s) {
        const float t0 = children[idx[s]].threshold;
        const float t1 = children[idx[s + 1]].threshold;
        if (x >= t0 && x <= t1) {
            const float denom = (t1 - t0);
            const float a = denom > 1e-6f ? (x - t0) / denom : 0.0f;
            w[idx[s]] = 1.0f - a;
            w[idx[s + 1]] = a;
            return w;
        }
    }

    w[idx.back()] = 1.0f;
    return w;
}

std::vector<float> BlendTree::Evaluate2D(const glm::vec2& xy) const {
    std::vector<float> w(children.size(), 0.0f);
    if (children.empty()) return w;
    if (children.size() == 1) { w[0] = 1.0f; return w; }

    // Production-style robust fallback: inverse-distance weighting.
    // This avoids brittle triangulation issues for arbitrary child layouts.
    const float eps = 1e-4f;
    float sum = 0.0f;
    for (size_t i = 0; i < children.size(); ++i) {
        const glm::vec2 d = xy - children[i].position;
        const float dist2 = glm::dot(d, d);
        const float wi = 1.0f / std::max(eps, dist2);
        w[i] = wi;
        sum += wi;
    }
    if (sum > 0.0f) {
        for (auto& wi : w) wi /= sum;
    } else {
        w[0] = 1.0f;
    }
    return w;
}

} // namespace Controller
} // namespace Animation
} // namespace OpenEye

