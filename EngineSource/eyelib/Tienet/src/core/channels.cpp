/**
 * @file channels.cpp
 * @brief Channel-based multiplexing implementation
 */

#include "tienet/core/channels.hpp"
#include <algorithm>
#include <cstring>

namespace tienet {
namespace core {

// Channel implementation
Channel::Channel(uint8_t channelId, const ChannelConfig& config)
    : m_channelId(channelId), m_config(config), m_queueSize(0), m_bytesSentThisSecond(0) {
    m_lastRateReset = std::chrono::steady_clock::now();
}

Channel::~Channel() {
    shutdown();
}

bool Channel::initialize() {
    m_sendQueue.clear();
    m_queueSize = 0;
    m_bytesSentThisSecond = 0;
    m_lastRateReset = std::chrono::steady_clock::now();
    
    return true;
}

void Channel::shutdown() {
    m_sendQueue.clear();
    m_queueSize = 0;
    m_bytesSentThisSecond = 0;
}

bool Channel::sendData(const void* data, size_t size) {
    if (size == 0 || !data) {
        return false;
    }
    
    // Check queue size limit
    if (m_queueSize >= m_config.maxQueueSize) {
        m_stats.packetsDropped++;
        return false;
    }
    
    // Prepare data for sending
    std::vector<uint8_t> packetData(static_cast<const uint8_t*>(data), 
                                   static_cast<const uint8_t*>(data) + size);
    
    // Compress if enabled
    if (m_config.enableCompression && size > 64) { // Only compress larger packets
        std::vector<uint8_t> compressed;
        if (compressData(packetData, compressed) && compressed.size() < packetData.size()) {
            packetData = std::move(compressed);
        }
    }
    
    // Add channel ID as first byte
    packetData.insert(packetData.begin(), m_channelId);
    
    // Queue for sending
    m_sendQueue.push_back(std::move(packetData));
    m_queueSize++;
    
    return true;
}

bool Channel::processPacket(const Packet& packet) {
    if (packet.getDataSize() == 0) {
        return false;
    }
    
    const auto& data = packet.getData();
    
    // Extract channel ID from first byte
    if (data[0] != m_channelId) {
        return false; // Not for this channel
    }
    
    // Extract actual data (skip channel ID byte)
    std::vector<uint8_t> channelData(data.begin() + 1, data.end());
    
    // Decompress if needed
    if (m_config.enableCompression) {
        std::vector<uint8_t> decompressed;
        if (decompressData(channelData, decompressed)) {
            channelData = std::move(decompressed);
        }
    }
    
    // Create packet for callback
    Packet channelPacket(packet.getType(), channelData.data(), channelData.size());
    channelPacket.setSequence(packet.getSequence());
    channelPacket.setFlags(packet.getFlags());
    
    // Update statistics
    m_stats.packetsReceived++;
    m_stats.bytesReceived += channelData.size();
    
    // Call data callback
    if (m_dataCallback) {
        m_dataCallback(channelPacket);
    }
    
    return true;
}

uint32_t Channel::update(uint32_t maxBytes) {
    if (m_sendQueue.empty()) {
        return 0;
    }
    
    updateRateLimiting();
    
    uint32_t bytesSent = 0;
    uint32_t bytesAllowed = maxBytes > 0 ? maxBytes : m_config.maxRateBps / 60; // Assume 60 FPS
    
    // Send queued packets
    while (!m_sendQueue.empty() && bytesSent < bytesAllowed) {
        auto& packetData = m_sendQueue.front();
        
        if (packetData.size() <= bytesAllowed - bytesSent) {
            // Create packet
            Packet packet(PacketType::DATA, packetData.data(), packetData.size());
            
            // Set appropriate flags based on delivery mode
            switch (m_config.deliveryMode) {
                case DeliveryMode::Unreliable:
                    // No flags needed
                    break;
                case DeliveryMode::Reliable:
                    packet.addFlag(PacketFlags::RELIABLE);
                    break;
                case DeliveryMode::Sequenced:
                    packet.addFlag(PacketFlags::RELIABLE);
                    packet.addFlag(PacketFlags::SEQUENCED);
                    break;
                case DeliveryMode::ReliableOrdered:
                    packet.addFlag(PacketFlags::RELIABLE);
                    packet.addFlag(PacketFlags::ORDERED);
                    break;
            }
            
            // Send packet (this would call the send callback)
            // For now, we'll just remove it from queue
            bytesSent += static_cast<uint32_t>(packetData.size());
            m_sendQueue.erase(m_sendQueue.begin());
            m_queueSize--;
            
            m_stats.packetsSent++;
            m_stats.bytesSent += packetData.size();
        } else {
            break; // Not enough bandwidth
        }
    }
    
    m_bytesSentThisSecond += bytesSent;
    return bytesSent;
}

void Channel::setDataCallback(DataCallback callback) {
    m_dataCallback = callback;
}

bool Channel::hasQueuedData() const {
    return !m_sendQueue.empty();
}

void Channel::updateRateLimiting() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRateReset);
    
    if (elapsed.count() >= 1000) { // Reset every second
        m_bytesSentThisSecond = 0;
        m_lastRateReset = now;
    }
}

bool Channel::compressData(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    // Simple compression placeholder - in real implementation, use LZ4 or similar
    // For now, just copy the data
    output = input;
    return true;
}

bool Channel::decompressData(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    // Simple decompression placeholder - in real implementation, use LZ4 or similar
    // For now, just copy the data
    output = input;
    return true;
}

// ChannelManager implementation
ChannelManager::ChannelManager() {
}

ChannelManager::~ChannelManager() {
    shutdown();
}

bool ChannelManager::initialize(SendCallback sendCallback) {
    m_sendCallback = sendCallback;
    return true;
}

void ChannelManager::shutdown() {
    m_channels.clear();
    m_sendCallback = nullptr;
}

bool ChannelManager::createChannel(uint8_t channelId, const ChannelConfig& config) {
    if (m_channels.find(channelId) != m_channels.end()) {
        return false; // Channel already exists
    }
    
    auto channel = std::make_shared<Channel>(channelId, config);
    if (!channel->initialize()) {
        return false;
    }
    
    m_channels[channelId] = channel;
    return true;
}

void ChannelManager::removeChannel(uint8_t channelId) {
    auto it = m_channels.find(channelId);
    if (it != m_channels.end()) {
        it->second->shutdown();
        m_channels.erase(it);
    }
}

bool ChannelManager::sendToChannel(uint8_t channelId, const void* data, size_t size) {
    auto it = m_channels.find(channelId);
    if (it != m_channels.end()) {
        return it->second->sendData(data, size);
    }
    return false;
}

bool ChannelManager::processPacket(const Packet& packet) {
    if (packet.getDataSize() == 0) {
        return false;
    }
    
    const auto& data = packet.getData();
    uint8_t channelId = data[0];
    
    auto it = m_channels.find(channelId);
    if (it != m_channels.end()) {
        return it->second->processPacket(packet);
    }
    
    return false;
}

uint32_t ChannelManager::update(uint32_t maxBytesPerChannel) {
    uint32_t totalBytesSent = 0;
    
    // Sort channels by priority
    std::vector<std::pair<uint8_t, std::shared_ptr<Channel>>> sortedChannels;
    for (const auto& pair : m_channels) {
        sortedChannels.push_back(pair);
    }
    
    std::sort(sortedChannels.begin(), sortedChannels.end(),
              [](const auto& a, const auto& b) {
                  return static_cast<int>(a.second->getConfig().priority) > 
                         static_cast<int>(b.second->getConfig().priority);
              });
    
    // Update channels in priority order
    for (const auto& pair : sortedChannels) {
        uint32_t bytesSent = pair.second->update(maxBytesPerChannel);
        totalBytesSent += bytesSent;
    }
    
    return totalBytesSent;
}

std::shared_ptr<Channel> ChannelManager::getChannel(uint8_t channelId) {
    auto it = m_channels.find(channelId);
    if (it != m_channels.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<uint8_t> ChannelManager::getActiveChannels() const {
    std::vector<uint8_t> channelIds;
    channelIds.reserve(m_channels.size());
    
    for (const auto& pair : m_channels) {
        channelIds.push_back(pair.first);
    }
    
    return channelIds;
}

void ChannelManager::setChannelDataCallback(uint8_t channelId, Channel::DataCallback callback) {
    auto channel = getChannel(channelId);
    if (channel) {
        channel->setDataCallback(callback);
    }
}

PacketType ChannelManager::getPacketTypeForDeliveryMode(DeliveryMode mode) {
    switch (mode) {
        case DeliveryMode::Unreliable:
            return PacketType::DATA;
        case DeliveryMode::Reliable:
        case DeliveryMode::Sequenced:
        case DeliveryMode::ReliableOrdered:
            return PacketType::DATA;
        default:
            return PacketType::DATA;
    }
}

PacketFlags ChannelManager::getPacketFlagsForDeliveryMode(DeliveryMode mode) {
    PacketFlags flags = PacketFlags::NONE;
    
    switch (mode) {
        case DeliveryMode::Reliable:
            flags = PacketFlags::RELIABLE;
            break;
        case DeliveryMode::Sequenced:
            flags = static_cast<PacketFlags>(static_cast<uint32_t>(PacketFlags::RELIABLE) | 
                                            static_cast<uint32_t>(PacketFlags::SEQUENCED));
            break;
        case DeliveryMode::ReliableOrdered:
            flags = static_cast<PacketFlags>(static_cast<uint32_t>(PacketFlags::RELIABLE) | 
                                            static_cast<uint32_t>(PacketFlags::ORDERED));
            break;
        case DeliveryMode::Unreliable:
        default:
            flags = PacketFlags::NONE;
            break;
    }
    
    return flags;
}

} // namespace core
} // namespace tienet
