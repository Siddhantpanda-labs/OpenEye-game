/**
 * @file priority_scheduler.hpp
 * @brief Advanced priority scheduling algorithms
 */

#pragma once

#include "tienet/core/channels.hpp"
#include <queue>
#include <vector>
#include <map>
#include <chrono>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <functional>

namespace tienet {
namespace core {

/**
 * @brief Scheduling algorithms
 */
enum class SchedulingAlgorithm {
    WeightedFairQueuing,    // WFQ
    DeficitRoundRobin,      // DRR
    PriorityBased,          // Simple priority
    LatencySensitive,       // LS scheduling
    Hybrid                  // Combination of algorithms
};

/**
 * @brief Packet scheduling information
 */
struct ScheduledPacket {
    uint8_t channelId;
    std::vector<uint8_t> data;
    uint32_t priority;
    uint32_t weight;
    uint32_t deadline;
    uint32_t timestamp;
    uint32_t size;
    
    ScheduledPacket() : channelId(0), priority(0), weight(1), deadline(0), timestamp(0), size(0) {}
};

/**
 * @brief Channel scheduling state
 */
struct ChannelSchedulingState {
    uint8_t channelId;
    uint32_t priority;
    uint32_t weight;
    uint32_t deficit;
    uint32_t quantum;
    uint32_t lastServiceTime;
    uint32_t packetsServed;
    uint32_t bytesServed;
    bool isActive;
    
    ChannelSchedulingState() : channelId(0), priority(0), weight(1), deficit(0), quantum(0),
                              lastServiceTime(0), packetsServed(0), bytesServed(0), isActive(false) {}
};

/**
 * @brief Scheduling statistics
 */
struct SchedulingStats {
    uint64_t totalPacketsScheduled;
    uint64_t totalBytesScheduled;
    uint32_t averageLatencyMs;
    uint32_t maxLatencyMs;
    uint32_t packetsDropped;
    uint32_t bytesDropped;
    std::map<uint8_t, uint32_t> channelStats;
    
    SchedulingStats() : totalPacketsScheduled(0), totalBytesScheduled(0),
                       averageLatencyMs(0), maxLatencyMs(0), packetsDropped(0), bytesDropped(0) {}
};

/**
 * @brief Abstract scheduler interface
 */
class IScheduler {
public:
    virtual ~IScheduler() = default;
    
    /**
     * @brief Enqueue packet for scheduling
     * @param packet Packet to schedule
     * @return true if enqueued, false if dropped
     */
    virtual bool enqueue(const ScheduledPacket& packet) = 0;
    
    /**
     * @brief Dequeue next packet to send
     * @param packet Output packet
     * @return true if packet available, false if queue empty
     */
    virtual bool dequeue(ScheduledPacket& packet) = 0;
    
    /**
     * @brief Update scheduler with new channel configuration
     * @param channelId Channel ID
     * @param config Channel configuration
     */
    virtual void updateChannel(uint8_t channelId, const ChannelConfig& config) = 0;
    
    /**
     * @brief Remove channel from scheduler
     * @param channelId Channel ID to remove
     */
    virtual void removeChannel(uint8_t channelId) = 0;
    
    /**
     * @brief Get scheduling statistics
     * @return Scheduling statistics
     */
    virtual SchedulingStats getStats() const = 0;
    
    /**
     * @brief Reset scheduler
     */
    virtual void reset() = 0;
    
    /**
     * @brief Get queue size
     * @return Current queue size
     */
    virtual size_t getQueueSize() const = 0;
};

/**
 * @brief Weighted Fair Queuing (WFQ) scheduler
 */
class WFQScheduler : public IScheduler {
public:
    WFQScheduler();
    ~WFQScheduler() override = default;
    
    bool enqueue(const ScheduledPacket& packet) override;
    bool dequeue(ScheduledPacket& packet) override;
    void updateChannel(uint8_t channelId, const ChannelConfig& config) override;
    void removeChannel(uint8_t channelId) override;
    SchedulingStats getStats() const override { return m_stats; }
    void reset() override;
    size_t getQueueSize() const override;

private:
    struct WFQPacket {
        ScheduledPacket packet;
        uint32_t finishTime;
        uint32_t virtualTime;
        
        WFQPacket(const ScheduledPacket& p, uint32_t ft, uint32_t vt) 
            : packet(p), finishTime(ft), virtualTime(vt) {}
    };
    
    std::map<uint8_t, ChannelSchedulingState> m_channels;
    std::priority_queue<WFQPacket, std::vector<WFQPacket>, 
                       std::function<bool(const WFQPacket&, const WFQPacket&)>> m_queue;
    SchedulingStats m_stats;
    uint32_t m_virtualTime;
    uint32_t m_maxQueueSize;
    
    uint32_t calculateFinishTime(const ScheduledPacket& packet, uint32_t channelWeight);
    uint32_t calculateVirtualTime(uint32_t finishTime, uint32_t weight);
};

/**
 * @brief Deficit Round Robin (DRR) scheduler
 */
class DRRScheduler : public IScheduler {
public:
    DRRScheduler();
    ~DRRScheduler() override = default;
    
    bool enqueue(const ScheduledPacket& packet) override;
    bool dequeue(ScheduledPacket& packet) override;
    void updateChannel(uint8_t channelId, const ChannelConfig& config) override;
    void removeChannel(uint8_t channelId) override;
    SchedulingStats getStats() const override { return m_stats; }
    void reset() override;
    size_t getQueueSize() const override;

private:
    std::map<uint8_t, ChannelSchedulingState> m_channels;
    std::map<uint8_t, std::queue<ScheduledPacket>> m_channelQueues;
    std::vector<uint8_t> m_activeChannels;
    size_t m_currentChannelIndex;
    SchedulingStats m_stats;
    uint32_t m_maxQueueSize;
    
    void updateActiveChannels();
    uint8_t getNextActiveChannel();
};

/**
 * @brief Priority-based scheduler
 */
class PriorityScheduler : public IScheduler {
public:
    PriorityScheduler();
    ~PriorityScheduler() override = default;
    
    bool enqueue(const ScheduledPacket& packet) override;
    bool dequeue(ScheduledPacket& packet) override;
    void updateChannel(uint8_t channelId, const ChannelConfig& config) override;
    void removeChannel(uint8_t channelId) override;
    SchedulingStats getStats() const override { return m_stats; }
    void reset() override;
    size_t getQueueSize() const override;

private:
    std::map<uint8_t, ChannelSchedulingState> m_channels;
    std::map<uint32_t, std::queue<ScheduledPacket>> m_priorityQueues; // priority -> queue
    SchedulingStats m_stats;
    uint32_t m_maxQueueSize;
    
    uint32_t getChannelPriority(uint8_t channelId);
};

/**
 * @brief Latency-sensitive scheduler
 */
class LatencySensitiveScheduler : public IScheduler {
public:
    LatencySensitiveScheduler();
    ~LatencySensitiveScheduler() override = default;
    
    bool enqueue(const ScheduledPacket& packet) override;
    bool dequeue(ScheduledPacket& packet) override;
    void updateChannel(uint8_t channelId, const ChannelConfig& config) override;
    void removeChannel(uint8_t channelId) override;
    SchedulingStats getStats() const override { return m_stats; }
    void reset() override;
    size_t getQueueSize() const override;

private:
    struct LSPacket {
        ScheduledPacket packet;
        uint32_t urgency;
        uint32_t slackTime;
        
        LSPacket(const ScheduledPacket& p) : packet(p) {
            uint32_t currentTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
            slackTime = packet.deadline - currentTime;
            urgency = (slackTime < 100) ? 1000 : (1000 / (slackTime + 1));
        }
    };
    
    std::map<uint8_t, ChannelSchedulingState> m_channels;
    std::priority_queue<LSPacket, std::vector<LSPacket>,
                       std::function<bool(const LSPacket&, const LSPacket&)>> m_queue;
    SchedulingStats m_stats;
    uint32_t m_maxQueueSize;
};

/**
 * @brief Hybrid scheduler combining multiple algorithms
 */
class HybridScheduler : public IScheduler {
public:
    HybridScheduler();
    ~HybridScheduler() override = default;
    
    bool enqueue(const ScheduledPacket& packet) override;
    bool dequeue(ScheduledPacket& packet) override;
    void updateChannel(uint8_t channelId, const ChannelConfig& config) override;
    void removeChannel(uint8_t channelId) override;
    SchedulingStats getStats() const override;
    void reset() override;
    size_t getQueueSize() const override;
    
    /**
     * @brief Set hybrid parameters
     * @param enableWFQ Enable WFQ
     * @param enableDRR Enable DRR
     * @param enablePriority Enable priority scheduling
     * @param enableLS Enable latency-sensitive scheduling
     */
    void setHybridParams(bool enableWFQ, bool enableDRR, bool enablePriority, bool enableLS);

private:
    std::unique_ptr<WFQScheduler> m_wfqScheduler;
    std::unique_ptr<DRRScheduler> m_drrScheduler;
    std::unique_ptr<PriorityScheduler> m_priorityScheduler;
    std::unique_ptr<LatencySensitiveScheduler> m_lsScheduler;
    
    bool m_enableWFQ;
    bool m_enableDRR;
    bool m_enablePriority;
    bool m_enableLS;
    
    SchedulingAlgorithm chooseAlgorithm(const ScheduledPacket& packet);
};

/**
 * @brief Scheduler factory
 */
class SchedulerFactory {
public:
    /**
     * @brief Create scheduler
     * @param algorithm Scheduling algorithm
     * @return Scheduler instance
     */
    static std::unique_ptr<IScheduler> createScheduler(SchedulingAlgorithm algorithm);
    
    /**
     * @brief Get available algorithms
     * @return Vector of available algorithms
     */
    static std::vector<SchedulingAlgorithm> getAvailableAlgorithms();
};

} // namespace core
} // namespace tienet
