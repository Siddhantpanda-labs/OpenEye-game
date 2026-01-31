/**
 * @file congestion_control.hpp
 * @brief Advanced congestion control algorithms
 */

#pragma once

#include <chrono>
#include <vector>
#include <deque>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

namespace tienet {
namespace core {

/**
 * @brief Congestion control algorithms
 */
enum class CongestionAlgorithm {
    Cubic,
    BBR,
    Vegas,
    Reno
};

/**
 * @brief Network sample for congestion control
 */
struct NetworkSample {
    uint64_t bytesAcked;
    uint64_t bytesLost;
    uint32_t rttMs;
    uint32_t timestampMs;
    bool isProbe;
    
    NetworkSample() : bytesAcked(0), bytesLost(0), rttMs(0), timestampMs(0), isProbe(false) {}
};

/**
 * @brief Congestion control state
 */
struct CongestionState {
    uint32_t congestionWindow;      // Current congestion window
    uint32_t slowStartThreshold;   // Slow start threshold
    uint32_t minRtt;               // Minimum RTT observed
    uint32_t maxRtt;               // Maximum RTT observed
    uint32_t rttVariation;         // RTT variation
    uint32_t bytesInFlight;        // Bytes currently in flight
    uint32_t packetsLost;          // Total packets lost
    uint32_t packetsAcked;         // Total packets acked
    bool inSlowStart;              // Currently in slow start
    bool inRecovery;               // Currently in recovery
    uint32_t recoveryStartTime;    // Recovery start timestamp
    
    CongestionState() : congestionWindow(10), slowStartThreshold(65535),
                       minRtt(UINT32_MAX), maxRtt(0), rttVariation(0),
                       bytesInFlight(0), packetsLost(0), packetsAcked(0),
                       inSlowStart(true), inRecovery(false), recoveryStartTime(0) {}
};

/**
 * @brief Abstract congestion control algorithm
 */
class ICongestionController {
public:
    virtual ~ICongestionController() = default;
    
    /**
     * @brief Update congestion control with new sample
     * @param sample Network sample
     * @return New congestion window size
     */
    virtual uint32_t update(const NetworkSample& sample) = 0;
    
    /**
     * @brief Get current congestion window
     * @return Congestion window size
     */
    virtual uint32_t getCongestionWindow() const = 0;
    
    /**
     * @brief Get current state
     * @return Congestion state
     */
    virtual CongestionState getState() const = 0;
    
    /**
     * @brief Reset congestion control
     */
    virtual void reset() = 0;
    
    /**
     * @brief Get algorithm name
     * @return Algorithm name
     */
    virtual std::string getAlgorithmName() const = 0;
};

/**
 * @brief CUBIC congestion control implementation
 */
class CubicController : public ICongestionController {
public:
    CubicController();
    ~CubicController() override = default;
    
    uint32_t update(const NetworkSample& sample) override;
    uint32_t getCongestionWindow() const override { return m_state.congestionWindow; }
    CongestionState getState() const override { return m_state; }
    void reset() override;
    std::string getAlgorithmName() const override { return "CUBIC"; }

private:
    CongestionState m_state;
    uint32_t m_lastMaxCwnd;
    uint32_t m_lastCwndTime;
    uint32_t m_epochStart;
    bool m_epochStartSet;
    
    uint32_t cubicCwnd(uint32_t t);
    uint32_t cubicTargetCwnd();
};

/**
 * @brief BBR (Bottleneck Bandwidth and RTT) congestion control
 */
class BBRController : public ICongestionController {
public:
    BBRController();
    ~BBRController() override = default;
    
    uint32_t update(const NetworkSample& sample) override;
    uint32_t getCongestionWindow() const override { return m_state.congestionWindow; }
    CongestionState getState() const override { return m_state; }
    void reset() override;
    std::string getAlgorithmName() const override { return "BBR"; }

private:
    CongestionState m_state;
    uint32_t m_bwFilter[10];       // Bandwidth filter
    uint32_t m_rttFilter[10];      // RTT filter
    uint32_t m_bwFilterIndex;
    uint32_t m_rttFilterIndex;
    uint32_t m_pacingGain;
    uint32_t m_cwndGain;
    uint32_t m_lastProbeTime;
    uint32_t m_probeRttTime;
    bool m_probeRttDone;
    
    enum class BBRState {
        Startup,
        Drain,
        ProbeBW,
        ProbeRTT
    };
    BBRState m_bbrState;
    
    uint32_t calculateBandwidth();
    uint32_t calculateRTT();
    void updateFilters(const NetworkSample& sample);
    void updateState();
};

/**
 * @brief Vegas congestion control implementation
 */
class VegasController : public ICongestionController {
public:
    VegasController();
    ~VegasController() override = default;
    
    uint32_t update(const NetworkSample& sample) override;
    uint32_t getCongestionWindow() const override { return m_state.congestionWindow; }
    CongestionState getState() const override { return m_state; }
    void reset() override;
    std::string getAlgorithmName() const override { return "Vegas"; }

private:
    CongestionState m_state;
    uint32_t m_baseRtt;
    uint32_t m_alpha;
    uint32_t m_beta;
    uint32_t m_gamma;
    
    uint32_t calculateExpectedThroughput();
    uint32_t calculateActualThroughput();
};

/**
 * @brief Reno congestion control implementation
 */
class RenoController : public ICongestionController {
public:
    RenoController();
    ~RenoController() override = default;
    
    uint32_t update(const NetworkSample& sample) override;
    uint32_t getCongestionWindow() const override { return m_state.congestionWindow; }
    CongestionState getState() const override { return m_state; }
    void reset() override;
    std::string getAlgorithmName() const override { return "Reno"; }

private:
    CongestionState m_state;
    uint32_t m_duplicateAcks;
    uint32_t m_lastAckedSeq;
};

/**
 * @brief Congestion control factory
 */
class CongestionControlFactory {
public:
    /**
     * @brief Create congestion controller
     * @param algorithm Algorithm type
     * @return Congestion controller instance
     */
    static std::unique_ptr<ICongestionController> createController(CongestionAlgorithm algorithm);
    
    /**
     * @brief Get available algorithms
     * @return Vector of available algorithms
     */
    static std::vector<CongestionAlgorithm> getAvailableAlgorithms();
};

/**
 * @brief Adaptive congestion control that switches algorithms
 */
class AdaptiveCongestionController : public ICongestionController {
public:
    AdaptiveCongestionController();
    ~AdaptiveCongestionController() override = default;
    
    uint32_t update(const NetworkSample& sample) override;
    uint32_t getCongestionWindow() const override;
    CongestionState getState() const override;
    void reset() override;
    std::string getAlgorithmName() const override { return "Adaptive"; }
    
    /**
     * @brief Set adaptation parameters
     * @param enableBBR Enable BBR
     * @param enableCubic Enable CUBIC
     * @param enableVegas Enable Vegas
     * @param enableReno Enable Reno
     */
    void setAdaptationParams(bool enableBBR, bool enableCubic, bool enableVegas, bool enableReno);

private:
    std::unique_ptr<ICongestionController> m_currentController;
    std::vector<CongestionAlgorithm> m_enabledAlgorithms;
    std::vector<NetworkSample> m_recentSamples;
    uint32_t m_lastSwitchTime;
    uint32_t m_switchInterval;
    
    CongestionAlgorithm chooseBestAlgorithm();
    void switchAlgorithm(CongestionAlgorithm algorithm);
};

} // namespace core
} // namespace tienet
