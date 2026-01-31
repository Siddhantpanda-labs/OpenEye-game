/**
 * @file channels.hpp
 * @brief Channel-based multiplexing for different data streams
 */

#pragma once

#include "tienet/core/packet.hpp"
#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>
#include <atomic>

namespace tienet {
namespace core {

/**
 * @brief Channel delivery modes
 */
enum class DeliveryMode {
    Unreliable,     // Best effort, no guarantees
    Reliable,       // Guaranteed delivery, ordered
    Sequenced,      // Guaranteed delivery, unordered
    ReliableOrdered // Guaranteed delivery, ordered, no duplicates
};

/**
 * @brief Channel priorities
 */
enum class ChannelPriority {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/**
 * @brief Channel configuration
 */
struct ChannelConfig {
    DeliveryMode deliveryMode;
    ChannelPriority priority;
    uint32_t maxRateBps;        // Maximum bytes per second
    uint32_t maxQueueSize;      // Maximum queued packets
    bool enableCompression;     // Enable compression for this channel
    uint32_t compressionLevel; // Compression level (1-9)
    
    ChannelConfig() : deliveryMode(DeliveryMode::Reliable), priority(ChannelPriority::Normal),
                     maxRateBps(0), maxQueueSize(1000), enableCompression(false), compressionLevel(6) {}
};

/**
 * @brief Channel statistics
 */
struct ChannelStats {
    uint64_t packetsSent;
    uint64_t packetsReceived;
    uint64_t bytesSent;
    uint64_t bytesReceived;
    uint64_t packetsDropped;
    uint64_t packetsLost;
    uint32_t averageLatency;
    uint32_t queueSize;
    
    ChannelStats() : packetsSent(0), packetsReceived(0), bytesSent(0), bytesReceived(0),
                     packetsDropped(0), packetsLost(0), averageLatency(0), queueSize(0) {}
};

/**
 * @brief Individual channel for multiplexed data streams
 */
class Channel {
public:
    using DataCallback = std::function<void(const Packet& packet)>;
    
    Channel(uint8_t channelId, const ChannelConfig& config);
    ~Channel();
    
    /**
     * @brief Initialize channel
     * @return true if successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Shutdown channel
     */
    void shutdown();
    
    /**
     * @brief Send data through this channel
     * @param data Data to send
     * @param size Data size
     * @return true if queued successfully, false otherwise
     */
    bool sendData(const void* data, size_t size);
    
    /**
     * @brief Process incoming packet for this channel
     * @param packet Received packet
     * @return true if processed successfully, false otherwise
     */
    bool processPacket(const Packet& packet);
    
    /**
     * @brief Update channel (process queued packets, rate limiting)
     * @param maxBytes Maximum bytes to send this update
     * @return Number of bytes sent
     */
    uint32_t update(uint32_t maxBytes = 0);
    
    /**
     * @brief Get channel ID
     * @return Channel ID
     */
    uint8_t getChannelId() const { return m_channelId; }
    
    /**
     * @brief Get channel configuration
     * @return Channel configuration
     */
    ChannelConfig getConfig() const { return m_config; }
    
    /**
     * @brief Get channel statistics
     * @return Channel statistics
     */
    ChannelStats getStats() const { return m_stats; }
    
    /**
     * @brief Set data callback
     * @param callback Data callback function
     */
    void setDataCallback(DataCallback callback);
    
    /**
     * @brief Check if channel has queued data
     * @return true if channel has queued data, false otherwise
     */
    bool hasQueuedData() const;

private:
    uint8_t m_channelId;
    ChannelConfig m_config;
    ChannelStats m_stats;
    
    std::vector<std::vector<uint8_t>> m_sendQueue;
    std::atomic<uint32_t> m_queueSize;
    std::atomic<uint32_t> m_bytesSentThisSecond;
    std::chrono::steady_clock::time_point m_lastRateReset;
    
    DataCallback m_dataCallback;
    
    void updateRateLimiting();
    bool compressData(const std::vector<uint8_t>& input, std::vector<uint8_t>& output);
    bool decompressData(const std::vector<uint8_t>& input, std::vector<uint8_t>& output);
};

/**
 * @brief Channel manager for handling multiple channels
 */
class ChannelManager {
public:
    using SendCallback = std::function<bool(const Packet& packet)>;
    
    ChannelManager();
    ~ChannelManager();
    
    /**
     * @brief Initialize channel manager
     * @param sendCallback Callback for sending packets
     * @return true if successful, false otherwise
     */
    bool initialize(SendCallback sendCallback);
    
    /**
     * @brief Shutdown channel manager
     */
    void shutdown();
    
    /**
     * @brief Create a new channel
     * @param channelId Channel ID (0-255)
     * @param config Channel configuration
     * @return true if successful, false otherwise
     */
    bool createChannel(uint8_t channelId, const ChannelConfig& config);
    
    /**
     * @brief Remove a channel
     * @param channelId Channel ID to remove
     */
    void removeChannel(uint8_t channelId);
    
    /**
     * @brief Send data through a specific channel
     * @param channelId Target channel ID
     * @param data Data to send
     * @param size Data size
     * @return true if queued successfully, false otherwise
     */
    bool sendToChannel(uint8_t channelId, const void* data, size_t size);
    
    /**
     * @brief Process incoming packet
     * @param packet Received packet
     * @return true if processed successfully, false otherwise
     */
    bool processPacket(const Packet& packet);
    
    /**
     * @brief Update all channels
     * @param maxBytesPerChannel Maximum bytes per channel this update
     * @return Total bytes sent
     */
    uint32_t update(uint32_t maxBytesPerChannel = 0);
    
    /**
     * @brief Get channel by ID
     * @param channelId Channel ID
     * @return Pointer to channel, nullptr if not found
     */
    std::shared_ptr<Channel> getChannel(uint8_t channelId);
    
    /**
     * @brief Get all active channel IDs
     * @return Vector of active channel IDs
     */
    std::vector<uint8_t> getActiveChannels() const;
    
    /**
     * @brief Set data callback for a channel
     * @param channelId Channel ID
     * @param callback Data callback function
     */
    void setChannelDataCallback(uint8_t channelId, Channel::DataCallback callback);

private:
    std::unordered_map<uint8_t, std::shared_ptr<Channel>> m_channels;
    SendCallback m_sendCallback;
    
    PacketType getPacketTypeForDeliveryMode(DeliveryMode mode);
    PacketFlags getPacketFlagsForDeliveryMode(DeliveryMode mode);
};

} // namespace core
} // namespace tienet
