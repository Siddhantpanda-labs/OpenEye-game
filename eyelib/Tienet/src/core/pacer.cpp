/**
 * @file pacer.cpp
 */

#include "tienet/core/pacer.hpp"

namespace tienet {
namespace core {

Pacer::Pacer()
    : m_targetBps(4 * 1024 * 1024), m_overheadBytes(28), m_tokens(0.0) {
    m_lastRefill = std::chrono::steady_clock::now();
}

void Pacer::setTargetBitrate(uint32_t bps) {
    m_targetBps = bps;
}

void Pacer::setDatagramOverhead(uint32_t bytes) {
    m_overheadBytes = bytes;
}

void Pacer::enqueue(std::vector<uint8_t>&& datagram) {
    m_queue.push_back(PendingDatagram{ std::move(datagram), std::chrono::steady_clock::now() });
}

uint32_t Pacer::dequeueBudgetBytes() {
    auto now = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration<double>(now - m_lastRefill).count();
    m_lastRefill = now;

    // Token-bucket: add tokens based on target bitrate
    m_tokens += (static_cast<double>(m_targetBps) / 8.0) * elapsedSec; // bytes
    if (m_tokens > static_cast<double>(m_targetBps)) {
        // Cap tokens at 1s worth to avoid bursts
        m_tokens = static_cast<double>(m_targetBps);
    }
    return static_cast<uint32_t>(m_tokens);
}

bool Pacer::hasPending() const {
    return !m_queue.empty();
}

PendingDatagram Pacer::pop() {
    PendingDatagram p = std::move(m_queue.front());
    m_queue.pop_front();
    // Spend tokens
    m_tokens = std::max(0.0, m_tokens - static_cast<double>(p.bytes.size() + m_overheadBytes));
    return p;
}

} // namespace core
} // namespace tienet


