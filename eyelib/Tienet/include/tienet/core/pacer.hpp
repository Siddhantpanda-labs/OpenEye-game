/**
 * @file pacer.hpp
 * @brief Packet pacing and send-queue for Tienet
 */

#pragma once

#include <cstdint>
#include <deque>
#include <vector>
#include <chrono>

namespace tienet {
namespace core {

struct PendingDatagram {
    std::vector<uint8_t> bytes;
    std::chrono::steady_clock::time_point enqueueTime;
};

class Pacer {
public:
    Pacer();

    void setTargetBitrate(uint32_t bps);
    void setDatagramOverhead(uint32_t bytes);

    void enqueue(std::vector<uint8_t>&& datagram);
    // Returns how many can be sent now based on pacing budget
    uint32_t dequeueBudgetBytes();
    bool hasPending() const;
    PendingDatagram pop();

private:
    uint32_t m_targetBps;
    uint32_t m_overheadBytes;
    double m_tokens; // bytes
    std::chrono::steady_clock::time_point m_lastRefill;
    std::deque<PendingDatagram> m_queue;
};

} // namespace core
} // namespace tienet


