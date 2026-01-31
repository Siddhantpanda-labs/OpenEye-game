/**
 * @file channel_pool.hpp
 * @brief Channel pooling and dynamic allocation
 */

#pragma once

#include "tienet/core/channels.hpp"
#include <vector>
#include <queue>
#include <unordered_map>
#include <mutex>

namespace tienet {
namespace core {

class ChannelPool {
public:
    ChannelPool();
    explicit ChannelPool(uint8_t maxChannels);

    uint8_t allocate(const ChannelConfig& config, bool* ok);
    void release(uint8_t channelId);
    bool update(uint8_t channelId, const ChannelConfig& config);
    bool exists(uint8_t channelId) const;
    size_t size() const;

private:
    uint8_t m_maxChannels;
    std::queue<uint8_t> m_free;
    std::unordered_map<uint8_t, ChannelConfig> m_active;
    mutable std::mutex m_mutex;

    void initFreeList();
};

} // namespace core
} // namespace tienet


