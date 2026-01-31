/**
 * @file channel_pool.cpp
 */

#include "tienet/core/channel_pool.hpp"

namespace tienet {
namespace core {

ChannelPool::ChannelPool() : m_maxChannels(255) { initFreeList(); }
ChannelPool::ChannelPool(uint8_t maxChannels) : m_maxChannels(maxChannels) { initFreeList(); }

void ChannelPool::initFreeList() {
    while (!m_free.empty()) m_free.pop();
    for (uint16_t i = 1; i <= m_maxChannels; ++i) m_free.push(static_cast<uint8_t>(i));
}

uint8_t ChannelPool::allocate(const ChannelConfig& config, bool* ok) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_free.empty()) { if (ok) *ok = false; return 0; }
    uint8_t id = m_free.front(); m_free.pop();
    m_active[id] = config;
    if (ok) *ok = true;
    return id;
}

void ChannelPool::release(uint8_t channelId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_active.find(channelId);
    if (it != m_active.end()) {
        m_active.erase(it);
        m_free.push(channelId);
    }
}

bool ChannelPool::update(uint8_t channelId, const ChannelConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_active.find(channelId);
    if (it == m_active.end()) return false;
    it->second = config;
    return true;
}

bool ChannelPool::exists(uint8_t channelId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_active.find(channelId) != m_active.end();
}

size_t ChannelPool::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_active.size();
}

} // namespace core
} // namespace tienet


