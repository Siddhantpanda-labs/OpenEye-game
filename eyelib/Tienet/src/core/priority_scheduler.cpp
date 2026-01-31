/**
 * @file priority_scheduler.cpp
 * @brief Advanced priority scheduling implementation
 */

#include "tienet/core/priority_scheduler.hpp"
#include <algorithm>
#include <numeric>

namespace tienet {
namespace core {

// WFQ Scheduler implementation
WFQScheduler::WFQScheduler() 
    : m_virtualTime(0), m_maxQueueSize(10000) {
    m_queue = std::priority_queue<WFQPacket, std::vector<WFQPacket>,
                                 std::function<bool(const WFQPacket&, const WFQPacket&)>>(
        [](const WFQPacket& a, const WFQPacket& b) {
            return a.finishTime > b.finishTime; // Min-heap
        });
}

bool WFQScheduler::enqueue(const ScheduledPacket& packet) {
    if (m_queue.size() >= m_maxQueueSize) {
        m_stats.packetsDropped++;
        m_stats.bytesDropped += packet.size;
        return false;
    }
    
    auto it = m_channels.find(packet.channelId);
    if (it == m_channels.end()) {
        return false; // Channel not registered
    }
    
    uint32_t weight = it->second.weight;
    uint32_t finishTime = calculateFinishTime(packet, weight);
    uint32_t virtualTime = calculateVirtualTime(finishTime, weight);
    
    m_queue.emplace(packet, finishTime, virtualTime);
    return true;
}

bool WFQScheduler::dequeue(ScheduledPacket& packet) {
    if (m_queue.empty()) {
        return false;
    }
    
    WFQPacket wfqPacket = m_queue.top();
    m_queue.pop();
    
    packet = wfqPacket.packet;
    
    // Update statistics
    m_stats.totalPacketsScheduled++;
    m_stats.totalBytesScheduled += packet.size;
    
    auto it = m_channels.find(packet.channelId);
    if (it != m_channels.end()) {
        it->second.packetsServed++;
        it->second.bytesServed += packet.size;
        it->second.lastServiceTime = packet.timestamp;
    }
    
    return true;
}

void WFQScheduler::updateChannel(uint8_t channelId, const ChannelConfig& config) {
    ChannelSchedulingState state;
    state.channelId = channelId;
    state.priority = static_cast<uint32_t>(config.priority);
    state.weight = std::max(1U, config.maxRateBps / 1000000); // Convert to weight
    state.quantum = state.weight * 1500; // MTU-based quantum
    state.isActive = true;
    
    m_channels[channelId] = state;
}

void WFQScheduler::removeChannel(uint8_t channelId) {
    m_channels.erase(channelId);
}

void WFQScheduler::reset() {
    while (!m_queue.empty()) {
        m_queue.pop();
    }
    m_channels.clear();
    m_stats = SchedulingStats();
    m_virtualTime = 0;
}

size_t WFQScheduler::getQueueSize() const {
    return m_queue.size();
}

uint32_t WFQScheduler::calculateFinishTime(const ScheduledPacket& packet, uint32_t channelWeight) {
    uint32_t currentTime = packet.timestamp;
    uint32_t serviceTime = packet.size / std::max(1U, channelWeight);
    return currentTime + serviceTime;
}

uint32_t WFQScheduler::calculateVirtualTime(uint32_t finishTime, uint32_t weight) {
    return finishTime / std::max(1U, weight);
}

// DRR Scheduler implementation
DRRScheduler::DRRScheduler() 
    : m_currentChannelIndex(0), m_maxQueueSize(10000) {
}

bool DRRScheduler::enqueue(const ScheduledPacket& packet) {
    auto& channelQueue = m_channelQueues[packet.channelId];
    
    if (channelQueue.size() >= m_maxQueueSize) {
        m_stats.packetsDropped++;
        m_stats.bytesDropped += packet.size;
        return false;
    }
    
    channelQueue.push(packet);
    updateActiveChannels();
    return true;
}

bool DRRScheduler::dequeue(ScheduledPacket& packet) {
    if (m_activeChannels.empty()) {
        return false;
    }
    
    uint8_t channelId = getNextActiveChannel();
    auto& channelQueue = m_channelQueues[channelId];
    
    if (channelQueue.empty()) {
        updateActiveChannels();
        return false;
    }
    
    packet = channelQueue.front();
    channelQueue.pop();
    
    // Update deficit
    auto& channelState = m_channels[channelId];
    channelState.deficit += channelState.quantum;
    
    // Update statistics
    m_stats.totalPacketsScheduled++;
    m_stats.totalBytesScheduled += packet.size;
    channelState.packetsServed++;
    channelState.bytesServed += packet.size;
    channelState.lastServiceTime = packet.timestamp;
    
    return true;
}

void DRRScheduler::updateChannel(uint8_t channelId, const ChannelConfig& config) {
    ChannelSchedulingState state;
    state.channelId = channelId;
    state.priority = static_cast<uint32_t>(config.priority);
    state.weight = std::max(1U, config.maxRateBps / 1000000);
    state.quantum = state.weight * 1500;
    state.deficit = 0;
    state.isActive = true;
    
    m_channels[channelId] = state;
    updateActiveChannels();
}

void DRRScheduler::removeChannel(uint8_t channelId) {
    m_channels.erase(channelId);
    m_channelQueues.erase(channelId);
    updateActiveChannels();
}

void DRRScheduler::reset() {
    m_channels.clear();
    m_channelQueues.clear();
    m_activeChannels.clear();
    m_stats = SchedulingStats();
    m_currentChannelIndex = 0;
}

size_t DRRScheduler::getQueueSize() const {
    size_t totalSize = 0;
    for (const auto& pair : m_channelQueues) {
        totalSize += pair.second.size();
    }
    return totalSize;
}

void DRRScheduler::updateActiveChannels() {
    m_activeChannels.clear();
    for (const auto& pair : m_channels) {
        if (pair.second.isActive && !m_channelQueues[pair.first].empty()) {
            m_activeChannels.push_back(pair.first);
        }
    }
}

uint8_t DRRScheduler::getNextActiveChannel() {
    if (m_activeChannels.empty()) {
        return 0;
    }
    
    uint8_t channelId = m_activeChannels[m_currentChannelIndex];
    m_currentChannelIndex = (m_currentChannelIndex + 1) % m_activeChannels.size();
    
    return channelId;
}

// Priority Scheduler implementation
PriorityScheduler::PriorityScheduler() 
    : m_maxQueueSize(10000) {
}

bool PriorityScheduler::enqueue(const ScheduledPacket& packet) {
    uint32_t priority = getChannelPriority(packet.channelId);
    
    if (m_priorityQueues[priority].size() >= m_maxQueueSize) {
        m_stats.packetsDropped++;
        m_stats.bytesDropped += packet.size;
        return false;
    }
    
    m_priorityQueues[priority].push(packet);
    return true;
}

bool PriorityScheduler::dequeue(ScheduledPacket& packet) {
    // Find highest priority non-empty queue
    for (auto it = m_priorityQueues.rbegin(); it != m_priorityQueues.rend(); ++it) {
        if (!it->second.empty()) {
            packet = it->second.front();
            it->second.pop();
            
            // Update statistics
            m_stats.totalPacketsScheduled++;
            m_stats.totalBytesScheduled += packet.size;
            
            auto channelIt = m_channels.find(packet.channelId);
            if (channelIt != m_channels.end()) {
                channelIt->second.packetsServed++;
                channelIt->second.bytesServed += packet.size;
                channelIt->second.lastServiceTime = packet.timestamp;
            }
            
            return true;
        }
    }
    
    return false;
}

void PriorityScheduler::updateChannel(uint8_t channelId, const ChannelConfig& config) {
    ChannelSchedulingState state;
    state.channelId = channelId;
    state.priority = static_cast<uint32_t>(config.priority);
    state.isActive = true;
    
    m_channels[channelId] = state;
}

void PriorityScheduler::removeChannel(uint8_t channelId) {
    m_channels.erase(channelId);
}

void PriorityScheduler::reset() {
    m_channels.clear();
    m_priorityQueues.clear();
    m_stats = SchedulingStats();
}

size_t PriorityScheduler::getQueueSize() const {
    size_t totalSize = 0;
    for (const auto& pair : m_priorityQueues) {
        totalSize += pair.second.size();
    }
    return totalSize;
}

uint32_t PriorityScheduler::getChannelPriority(uint8_t channelId) {
    auto it = m_channels.find(channelId);
    if (it != m_channels.end()) {
        return it->second.priority;
    }
    return 0; // Default priority
}

// Latency Sensitive Scheduler implementation
LatencySensitiveScheduler::LatencySensitiveScheduler() 
    : m_maxQueueSize(10000) {
    m_queue = std::priority_queue<LSPacket, std::vector<LSPacket>,
                                 std::function<bool(const LSPacket&, const LSPacket&)>>(
        [](const LSPacket& a, const LSPacket& b) {
            return a.urgency < b.urgency; // Min-heap (highest urgency first)
        });
}

bool LatencySensitiveScheduler::enqueue(const ScheduledPacket& packet) {
    if (m_queue.size() >= m_maxQueueSize) {
        m_stats.packetsDropped++;
        m_stats.bytesDropped += packet.size;
        return false;
    }
    
    m_queue.emplace(packet);
    return true;
}

bool LatencySensitiveScheduler::dequeue(ScheduledPacket& packet) {
    if (m_queue.empty()) {
        return false;
    }
    
    LSPacket lsPacket = m_queue.top();
    m_queue.pop();
    
    packet = lsPacket.packet;
    
    // Update statistics
    m_stats.totalPacketsScheduled++;
    m_stats.totalBytesScheduled += packet.size;
    
    auto it = m_channels.find(packet.channelId);
    if (it != m_channels.end()) {
        it->second.packetsServed++;
        it->second.bytesServed += packet.size;
        it->second.lastServiceTime = packet.timestamp;
    }
    
    return true;
}

void LatencySensitiveScheduler::updateChannel(uint8_t channelId, const ChannelConfig& config) {
    ChannelSchedulingState state;
    state.channelId = channelId;
    state.priority = static_cast<uint32_t>(config.priority);
    state.isActive = true;
    
    m_channels[channelId] = state;
}

void LatencySensitiveScheduler::removeChannel(uint8_t channelId) {
    m_channels.erase(channelId);
}

void LatencySensitiveScheduler::reset() {
    while (!m_queue.empty()) {
        m_queue.pop();
    }
    m_channels.clear();
    m_stats = SchedulingStats();
}

size_t LatencySensitiveScheduler::getQueueSize() const {
    return m_queue.size();
}

// Hybrid Scheduler implementation
HybridScheduler::HybridScheduler() 
    : m_enableWFQ(true), m_enableDRR(true), m_enablePriority(true), m_enableLS(true) {
    m_wfqScheduler = std::make_unique<WFQScheduler>();
    m_drrScheduler = std::make_unique<DRRScheduler>();
    m_priorityScheduler = std::make_unique<PriorityScheduler>();
    m_lsScheduler = std::make_unique<LatencySensitiveScheduler>();
}

bool HybridScheduler::enqueue(const ScheduledPacket& packet) {
    SchedulingAlgorithm algorithm = chooseAlgorithm(packet);
    
    switch (algorithm) {
        case SchedulingAlgorithm::WeightedFairQueuing:
            return m_wfqScheduler->enqueue(packet);
        case SchedulingAlgorithm::DeficitRoundRobin:
            return m_drrScheduler->enqueue(packet);
        case SchedulingAlgorithm::PriorityBased:
            return m_priorityScheduler->enqueue(packet);
        case SchedulingAlgorithm::LatencySensitive:
            return m_lsScheduler->enqueue(packet);
        default:
            return m_priorityScheduler->enqueue(packet);
    }
}

bool HybridScheduler::dequeue(ScheduledPacket& packet) {
    // Try schedulers in order of preference
    if (m_enableLS && m_lsScheduler->dequeue(packet)) {
        return true;
    }
    if (m_enablePriority && m_priorityScheduler->dequeue(packet)) {
        return true;
    }
    if (m_enableWFQ && m_wfqScheduler->dequeue(packet)) {
        return true;
    }
    if (m_enableDRR && m_drrScheduler->dequeue(packet)) {
        return true;
    }
    
    return false;
}

void HybridScheduler::updateChannel(uint8_t channelId, const ChannelConfig& config) {
    m_wfqScheduler->updateChannel(channelId, config);
    m_drrScheduler->updateChannel(channelId, config);
    m_priorityScheduler->updateChannel(channelId, config);
    m_lsScheduler->updateChannel(channelId, config);
}

void HybridScheduler::removeChannel(uint8_t channelId) {
    m_wfqScheduler->removeChannel(channelId);
    m_drrScheduler->removeChannel(channelId);
    m_priorityScheduler->removeChannel(channelId);
    m_lsScheduler->removeChannel(channelId);
}

SchedulingStats HybridScheduler::getStats() const {
    SchedulingStats combined;
    auto wfqStats = m_wfqScheduler->getStats();
    auto drrStats = m_drrScheduler->getStats();
    auto priorityStats = m_priorityScheduler->getStats();
    auto lsStats = m_lsScheduler->getStats();
    
    combined.totalPacketsScheduled = wfqStats.totalPacketsScheduled + drrStats.totalPacketsScheduled +
                                   priorityStats.totalPacketsScheduled + lsStats.totalPacketsScheduled;
    combined.totalBytesScheduled = wfqStats.totalBytesScheduled + drrStats.totalBytesScheduled +
                                  priorityStats.totalBytesScheduled + lsStats.totalBytesScheduled;
    combined.packetsDropped = wfqStats.packetsDropped + drrStats.packetsDropped +
                             priorityStats.packetsDropped + lsStats.packetsDropped;
    combined.bytesDropped = wfqStats.bytesDropped + drrStats.bytesDropped +
                           priorityStats.bytesDropped + lsStats.bytesDropped;
    
    return combined;
}

void HybridScheduler::reset() {
    m_wfqScheduler->reset();
    m_drrScheduler->reset();
    m_priorityScheduler->reset();
    m_lsScheduler->reset();
}

size_t HybridScheduler::getQueueSize() const {
    return m_wfqScheduler->getQueueSize() + m_drrScheduler->getQueueSize() +
           m_priorityScheduler->getQueueSize() + m_lsScheduler->getQueueSize();
}

void HybridScheduler::setHybridParams(bool enableWFQ, bool enableDRR, bool enablePriority, bool enableLS) {
    m_enableWFQ = enableWFQ;
    m_enableDRR = enableDRR;
    m_enablePriority = enablePriority;
    m_enableLS = enableLS;
}

SchedulingAlgorithm HybridScheduler::chooseAlgorithm(const ScheduledPacket& packet) {
    // Simple algorithm selection based on packet characteristics
    if (packet.deadline > 0 && packet.deadline < 100) {
        return SchedulingAlgorithm::LatencySensitive;
    }
    
    if (packet.priority >= 3) {
        return SchedulingAlgorithm::PriorityBased;
    }
    
    if (packet.weight > 1) {
        return SchedulingAlgorithm::WeightedFairQueuing;
    }
    
    return SchedulingAlgorithm::DeficitRoundRobin;
}

// Scheduler Factory implementation
std::unique_ptr<IScheduler> SchedulerFactory::createScheduler(SchedulingAlgorithm algorithm) {
    switch (algorithm) {
        case SchedulingAlgorithm::WeightedFairQueuing:
            return std::make_unique<WFQScheduler>();
        case SchedulingAlgorithm::DeficitRoundRobin:
            return std::make_unique<DRRScheduler>();
        case SchedulingAlgorithm::PriorityBased:
            return std::make_unique<PriorityScheduler>();
        case SchedulingAlgorithm::LatencySensitive:
            return std::make_unique<LatencySensitiveScheduler>();
        case SchedulingAlgorithm::Hybrid:
            return std::make_unique<HybridScheduler>();
        default:
            return std::make_unique<PriorityScheduler>();
    }
}

std::vector<SchedulingAlgorithm> SchedulerFactory::getAvailableAlgorithms() {
    return {
        SchedulingAlgorithm::WeightedFairQueuing,
        SchedulingAlgorithm::DeficitRoundRobin,
        SchedulingAlgorithm::PriorityBased,
        SchedulingAlgorithm::LatencySensitive,
        SchedulingAlgorithm::Hybrid
    };
}

} // namespace core
} // namespace tienet
