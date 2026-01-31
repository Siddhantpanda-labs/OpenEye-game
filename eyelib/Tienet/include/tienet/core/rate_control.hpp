/**
 * @file rate_control.hpp
 * @brief Congestion/rate control abstraction for Tienet
 */

#pragma once

#include <cstdint>

namespace tienet {
namespace core {

struct RateSample {
    uint32_t rttMs = 0;
    float lossPercent = 0.0f; // 0..100
    uint64_t bytesAcked = 0;  // bytes acknowledged in window
    uint32_t windowMs = 1000; // measurement window
};

class RateController {
public:
    RateController();

    void setBounds(uint32_t minBps, uint32_t maxBps);
    void setInitial(uint32_t initialBps);

    // Update with the latest measurement and get recommended target bitrate
    uint32_t update(const RateSample& sampleMs);

    uint32_t getTargetBitrate() const { return m_targetBps; }

private:
    uint32_t m_minBps;
    uint32_t m_maxBps;
    uint32_t m_targetBps;

    // Advanced control state
    float m_lossEwma = 0.0f;     // smoothed loss
    float m_rttEwmaMs = 0.0f;    // smoothed rtt
    uint32_t m_probeState = 0;   // 0 steady, 1 up, 2 down
    uint32_t m_lastProbeBps = 0; // last probed rate
    uint32_t m_probeCooldownMs = 0;
    uint32_t m_baseRttMs = 0;    // min observed RTT
};

} // namespace core
} // namespace tienet


