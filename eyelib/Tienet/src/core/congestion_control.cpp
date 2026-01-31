/**
 * @file congestion_control.cpp
 * @brief Advanced congestion control implementation
 */

#include "tienet/core/congestion_control.hpp"
#include <algorithm>
#include <numeric>

namespace tienet {
namespace core {

// CUBIC Controller implementation
CubicController::CubicController() 
    : m_lastMaxCwnd(0), m_lastCwndTime(0), m_epochStart(0), m_epochStartSet(false) {
}

uint32_t CubicController::update(const NetworkSample& sample) {
    uint32_t currentTime = sample.timestampMs;
    
    // Update RTT statistics
    if (sample.rttMs < m_state.minRtt) {
        m_state.minRtt = sample.rttMs;
    }
    if (sample.rttMs > m_state.maxRtt) {
        m_state.maxRtt = sample.rttMs;
    }
    
    // Update packet statistics
    m_state.packetsAcked += 1;
    m_state.packetsLost += sample.bytesLost > 0 ? 1 : 0;
    
    if (sample.bytesLost > 0) {
        // Packet loss detected
        if (m_state.inSlowStart) {
            m_state.slowStartThreshold = m_state.congestionWindow / 2;
            m_state.congestionWindow = m_state.slowStartThreshold;
            m_state.inSlowStart = false;
        } else {
            // CUBIC reduction
            m_lastMaxCwnd = m_state.congestionWindow;
            m_state.congestionWindow = static_cast<uint32_t>(m_state.congestionWindow * 0.7);
        }
        
        m_state.inRecovery = true;
        m_state.recoveryStartTime = currentTime;
    } else if (sample.bytesAcked > 0) {
        // Packet acknowledged
        if (m_state.inRecovery && currentTime - m_state.recoveryStartTime > m_state.minRtt) {
            m_state.inRecovery = false;
        }
        
        if (m_state.inSlowStart) {
            // Slow start
            m_state.congestionWindow += sample.bytesAcked;
            if (m_state.congestionWindow >= m_state.slowStartThreshold) {
                m_state.inSlowStart = false;
            }
        } else {
            // CUBIC congestion avoidance
            uint32_t targetCwnd = cubicTargetCwnd();
            if (m_state.congestionWindow < targetCwnd) {
                m_state.congestionWindow += std::max(1U, (targetCwnd - m_state.congestionWindow) / m_state.congestionWindow);
            } else {
                m_state.congestionWindow += 1;
            }
        }
    }
    
    // Ensure minimum window size
    m_state.congestionWindow = std::max(2U, m_state.congestionWindow);
    
    return m_state.congestionWindow;
}

void CubicController::reset() {
    m_state = CongestionState();
    m_lastMaxCwnd = 0;
    m_lastCwndTime = 0;
    m_epochStart = 0;
    m_epochStartSet = false;
}

uint32_t CubicController::cubicCwnd(uint32_t t) {
    const double C = 0.4; // CUBIC parameter
    const double K = std::cbrt(m_lastMaxCwnd * (1.0 - 0.7) / C);
    
    return static_cast<uint32_t>(C * std::pow(t - K, 3) + m_lastMaxCwnd);
}

uint32_t CubicController::cubicTargetCwnd() {
    uint32_t currentTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    
    if (!m_epochStartSet) {
        m_epochStart = currentTime;
        m_epochStartSet = true;
    }
    
    uint32_t t = currentTime - m_epochStart;
    return cubicCwnd(t);
}

// BBR Controller implementation
BBRController::BBRController() 
    : m_bwFilterIndex(0), m_rttFilterIndex(0), m_pacingGain(256), m_cwndGain(256),
      m_lastProbeTime(0), m_probeRttTime(0), m_probeRttDone(false), m_bbrState(BBRState::Startup) {
    std::fill(m_bwFilter, m_bwFilter + 10, 0);
    std::fill(m_rttFilter, m_rttFilter + 10, UINT32_MAX);
}

uint32_t BBRController::update(const NetworkSample& sample) {
    updateFilters(sample);
    updateState();
    
    // Calculate pacing rate and congestion window
    uint32_t bw = calculateBandwidth();
    uint32_t rtt = calculateRTT();
    
    if (bw > 0 && rtt > 0) {
        // BBR formula: cwnd = bandwidth * RTT * gain
        uint32_t targetCwnd = (bw * rtt * m_cwndGain) / 1000000; // Convert to bytes
        m_state.congestionWindow = std::max(4U, targetCwnd);
    }
    
    return m_state.congestionWindow;
}

void BBRController::reset() {
    m_state = CongestionState();
    m_bwFilterIndex = 0;
    m_rttFilterIndex = 0;
    m_pacingGain = 256;
    m_cwndGain = 256;
    m_lastProbeTime = 0;
    m_probeRttTime = 0;
    m_probeRttDone = false;
    m_bbrState = BBRState::Startup;
    std::fill(m_bwFilter, m_bwFilter + 10, 0);
    std::fill(m_rttFilter, m_rttFilter + 10, UINT32_MAX);
}

uint32_t BBRController::calculateBandwidth() {
    // Return median bandwidth from filter
    std::vector<uint32_t> sortedBw(m_bwFilter, m_bwFilter + 10);
    std::sort(sortedBw.begin(), sortedBw.end());
    return sortedBw[5]; // Median
}

uint32_t BBRController::calculateRTT() {
    // Return minimum RTT from filter
    uint32_t minRtt = UINT32_MAX;
    for (int i = 0; i < 10; i++) {
        if (m_rttFilter[i] < minRtt) {
            minRtt = m_rttFilter[i];
        }
    }
    return minRtt == UINT32_MAX ? 100 : minRtt; // Default 100ms
}

void BBRController::updateFilters(const NetworkSample& sample) {
    // Update bandwidth filter
    if (sample.bytesAcked > 0 && sample.rttMs > 0) {
        uint32_t bw = (sample.bytesAcked * 1000) / sample.rttMs; // bytes per second
        m_bwFilter[m_bwFilterIndex] = bw;
        m_bwFilterIndex = (m_bwFilterIndex + 1) % 10;
    }
    
    // Update RTT filter
    if (sample.rttMs > 0) {
        m_rttFilter[m_rttFilterIndex] = sample.rttMs;
        m_rttFilterIndex = (m_rttFilterIndex + 1) % 10;
    }
}

void BBRController::updateState() {
    uint32_t currentTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    
    switch (m_bbrState) {
        case BBRState::Startup:
            if (m_state.congestionWindow >= m_state.slowStartThreshold) {
                m_bbrState = BBRState::Drain;
                m_cwndGain = 256; // 1.0x gain
            }
            break;
            
        case BBRState::Drain:
            if (m_state.bytesInFlight <= m_state.congestionWindow) {
                m_bbrState = BBRState::ProbeBW;
                m_cwndGain = 256; // 1.0x gain
            }
            break;
            
        case BBRState::ProbeBW:
            // Cycle through different gains
            if (currentTime - m_lastProbeTime > 10000) { // 10 seconds
                m_cwndGain = (m_cwndGain == 256) ? 512 : 256; // 1.0x or 2.0x
                m_lastProbeTime = currentTime;
            }
            break;
            
        case BBRState::ProbeRTT:
            if (currentTime - m_probeRttTime > 200) { // 200ms
                m_bbrState = BBRState::ProbeBW;
                m_cwndGain = 256;
                m_probeRttDone = false;
            }
            break;
    }
}

// Vegas Controller implementation
VegasController::VegasController() 
    : m_baseRtt(UINT32_MAX), m_alpha(1), m_beta(3), m_gamma(1) {
}

uint32_t VegasController::update(const NetworkSample& sample) {
    // Update base RTT
    if (sample.rttMs < m_baseRtt) {
        m_baseRtt = sample.rttMs;
    }
    
    if (sample.bytesAcked > 0) {
        uint32_t expected = calculateExpectedThroughput();
        uint32_t actual = calculateActualThroughput();
        
        if (actual < expected - m_alpha) {
            // Increase congestion window
            m_state.congestionWindow += 1;
        } else if (actual > expected + m_beta) {
            // Decrease congestion window
            m_state.congestionWindow = std::max(2U, m_state.congestionWindow - 1);
        }
    }
    
    if (sample.bytesLost > 0) {
        // Packet loss - reduce window
        m_state.congestionWindow = std::max(2U, m_state.congestionWindow / 2);
    }
    
    return m_state.congestionWindow;
}

void VegasController::reset() {
    m_state = CongestionState();
    m_baseRtt = UINT32_MAX;
}

uint32_t VegasController::calculateExpectedThroughput() {
    if (m_baseRtt == UINT32_MAX) return 0;
    return m_state.congestionWindow * 1000 / m_baseRtt;
}

uint32_t VegasController::calculateActualThroughput() {
    // Simplified calculation - in practice, you'd track actual throughput
    return m_state.packetsAcked * 1000 / (m_state.minRtt > 0 ? m_state.minRtt : 100);
}

// Reno Controller implementation
RenoController::RenoController() 
    : m_duplicateAcks(0), m_lastAckedSeq(0) {
}

uint32_t RenoController::update(const NetworkSample& sample) {
    if (sample.bytesLost > 0) {
        // Packet loss detected
        if (m_state.inSlowStart) {
            m_state.slowStartThreshold = m_state.congestionWindow / 2;
            m_state.congestionWindow = m_state.slowStartThreshold;
            m_state.inSlowStart = false;
        } else {
            // Fast recovery
            m_state.congestionWindow = std::max(2U, m_state.congestionWindow / 2);
            m_state.slowStartThreshold = m_state.congestionWindow;
        }
        
        m_state.inRecovery = true;
        m_state.recoveryStartTime = sample.timestampMs;
    } else if (sample.bytesAcked > 0) {
        // Packet acknowledged
        if (m_state.inRecovery && sample.timestampMs - m_state.recoveryStartTime > m_state.minRtt) {
            m_state.inRecovery = false;
        }
        
        if (m_state.inSlowStart) {
            // Slow start
            m_state.congestionWindow += sample.bytesAcked;
            if (m_state.congestionWindow >= m_state.slowStartThreshold) {
                m_state.inSlowStart = false;
            }
        } else {
            // Congestion avoidance
            m_state.congestionWindow += (sample.bytesAcked * sample.bytesAcked) / m_state.congestionWindow;
        }
    }
    
    return m_state.congestionWindow;
}

void RenoController::reset() {
    m_state = CongestionState();
    m_duplicateAcks = 0;
    m_lastAckedSeq = 0;
}

// Congestion Control Factory implementation
std::unique_ptr<ICongestionController> CongestionControlFactory::createController(CongestionAlgorithm algorithm) {
    switch (algorithm) {
        case CongestionAlgorithm::Cubic:
            return std::make_unique<CubicController>();
        case CongestionAlgorithm::BBR:
            return std::make_unique<BBRController>();
        case CongestionAlgorithm::Vegas:
            return std::make_unique<VegasController>();
        case CongestionAlgorithm::Reno:
            return std::make_unique<RenoController>();
        default:
            return std::make_unique<CubicController>();
    }
}

std::vector<CongestionAlgorithm> CongestionControlFactory::getAvailableAlgorithms() {
    return {
        CongestionAlgorithm::Cubic,
        CongestionAlgorithm::BBR,
        CongestionAlgorithm::Vegas,
        CongestionAlgorithm::Reno
    };
}

// Adaptive Congestion Controller implementation
AdaptiveCongestionController::AdaptiveCongestionController() 
    : m_lastSwitchTime(0), m_switchInterval(30000) { // 30 seconds
    m_enabledAlgorithms = {
        CongestionAlgorithm::BBR,
        CongestionAlgorithm::Cubic,
        CongestionAlgorithm::Vegas,
        CongestionAlgorithm::Reno
    };
    
    m_currentController = std::make_unique<BBRController>();
}

// Note: Destructor is defaulted in the header

uint32_t AdaptiveCongestionController::update(const NetworkSample& sample) {
    m_recentSamples.push_back(sample);
    if (m_recentSamples.size() > 100) {
        m_recentSamples.erase(m_recentSamples.begin());
    }
    
    uint32_t currentTime = sample.timestampMs;
    
    // Switch algorithm if needed
    if (currentTime - m_lastSwitchTime > m_switchInterval) {
        CongestionAlgorithm bestAlgorithm = chooseBestAlgorithm();
        if (bestAlgorithm != CongestionAlgorithm::Cubic) { // Default check
            switchAlgorithm(bestAlgorithm);
            m_lastSwitchTime = currentTime;
        }
    }
    
    return m_currentController->update(sample);
}

uint32_t AdaptiveCongestionController::getCongestionWindow() const {
    return m_currentController->getCongestionWindow();
}

CongestionState AdaptiveCongestionController::getState() const {
    return m_currentController->getState();
}

void AdaptiveCongestionController::reset() {
    m_currentController->reset();
    m_recentSamples.clear();
    m_lastSwitchTime = 0;
}

void AdaptiveCongestionController::setAdaptationParams(bool enableBBR, bool enableCubic, bool enableVegas, bool enableReno) {
    m_enabledAlgorithms.clear();
    
    if (enableBBR) m_enabledAlgorithms.push_back(CongestionAlgorithm::BBR);
    if (enableCubic) m_enabledAlgorithms.push_back(CongestionAlgorithm::Cubic);
    if (enableVegas) m_enabledAlgorithms.push_back(CongestionAlgorithm::Vegas);
    if (enableReno) m_enabledAlgorithms.push_back(CongestionAlgorithm::Reno);
    
    if (m_enabledAlgorithms.empty()) {
        m_enabledAlgorithms.push_back(CongestionAlgorithm::Cubic);
    }
}

CongestionAlgorithm AdaptiveCongestionController::chooseBestAlgorithm() {
    if (m_recentSamples.size() < 10) {
        return CongestionAlgorithm::Cubic; // Default
    }
    
    // Calculate performance metrics for each algorithm
    struct AlgorithmScore {
        CongestionAlgorithm algorithm;
        double score;
    };
    
    std::vector<AlgorithmScore> scores;
    
    for (auto algorithm : m_enabledAlgorithms) {
        auto controller = CongestionControlFactory::createController(algorithm);
        double score = 0.0;
        
        for (const auto& sample : m_recentSamples) {
            controller->update(sample);
            // Score based on throughput and loss rate
            double throughput = static_cast<double>(sample.bytesAcked) / sample.rttMs;
            double lossRate = static_cast<double>(sample.bytesLost) / (sample.bytesAcked + sample.bytesLost);
            score += throughput * (1.0 - lossRate);
        }
        
        scores.push_back({algorithm, score});
    }
    
    // Return algorithm with highest score
    auto best = std::max_element(scores.begin(), scores.end(),
        [](const AlgorithmScore& a, const AlgorithmScore& b) {
            return a.score < b.score;
        });
    
    return best->algorithm;
}

void AdaptiveCongestionController::switchAlgorithm(CongestionAlgorithm algorithm) {
    uint32_t currentCwnd = m_currentController->getCongestionWindow();
    m_currentController = CongestionControlFactory::createController(algorithm);
    
    // Transfer state
    auto state = m_currentController->getState();
    state.congestionWindow = currentCwnd;
    // Note: In a real implementation, you'd transfer more state
}

} // namespace core
} // namespace tienet
