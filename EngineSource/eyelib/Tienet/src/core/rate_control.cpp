/**
 * @file rate_control.cpp
 */

#include "tienet/core/rate_control.hpp"
#include <algorithm>

namespace tienet {
namespace core {

RateController::RateController()
    : m_minBps(128 * 1024), m_maxBps(100 * 1024 * 1024), m_targetBps(4 * 1024 * 1024) {}

void RateController::setBounds(uint32_t minBps, uint32_t maxBps) {
    m_minBps = std::min(minBps, maxBps);
    m_maxBps = std::max(minBps, maxBps);
    m_targetBps = std::clamp(m_targetBps, m_minBps, m_maxBps);
}

void RateController::setInitial(uint32_t initialBps) {
    m_targetBps = std::clamp(initialBps, m_minBps, m_maxBps);
}

uint32_t RateController::update(const RateSample& sample) {
    // EWMA smoothing
    const float alpha = 0.2f;
    m_lossEwma = (1.0f - alpha) * m_lossEwma + alpha * sample.lossPercent;
    m_rttEwmaMs = (1.0f - alpha) * m_rttEwmaMs + alpha * static_cast<float>(sample.rttMs);
    if (m_baseRttMs == 0 || sample.rttMs < m_baseRttMs) m_baseRttMs = sample.rttMs;

    float target = static_cast<float>(m_targetBps);

    // Loss-based adjustment (delay-based guard using base RTT)
    bool congested = (m_lossEwma > 2.0f) || (m_rttEwmaMs > std::max<float>(150.0f, 1.5f * static_cast<float>(m_baseRttMs)));
    if (congested) {
        // Multiplicative decrease
        target *= 0.85f;
        m_probeState = 2; // down
        m_probeCooldownMs = 1000;
    } else {
        // Additive increase with occasional probing
        if (m_probeCooldownMs > 0) {
            m_probeCooldownMs = (m_probeCooldownMs > sample.windowMs) ? (m_probeCooldownMs - sample.windowMs) : 0;
        }
        // Only probe up if recent loss is low
        if (m_probeCooldownMs == 0 && m_probeState != 1 && m_lossEwma < 1.0f) {
            // Try an up-probe of +15%
            m_lastProbeBps = static_cast<uint32_t>(target * 1.15f);
            m_probeState = 1;
            target = static_cast<float>(m_lastProbeBps);
            m_probeCooldownMs = 1500;
        } else {
            target *= 1.05f;
        }
    }

    m_targetBps = static_cast<uint32_t>(std::clamp(target, static_cast<float>(m_minBps), static_cast<float>(m_maxBps)));
    return m_targetBps;
}

} // namespace core
} // namespace tienet


