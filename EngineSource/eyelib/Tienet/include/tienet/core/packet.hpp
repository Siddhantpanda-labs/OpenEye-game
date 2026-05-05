/**
 * @file packet.hpp
 * @brief Generic packet system for network communication
 */

#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <chrono>

namespace tienet {
namespace core {

/**
 * @brief Packet types for different kinds of data
 */
enum class PacketType : uint8_t {
    // Connection management
    CONNECT_REQUEST = 0x01,
    CONNECT_RESPONSE = 0x02,
    DISCONNECT = 0x03,
    
    // Heartbeat
    PING = 0x10,
    PONG = 0x11,
    
    // Data transmission
    DATA = 0x20,
    FRAGMENT = 0x21,
    ACK = 0x22,
    NACK = 0x23,
    FEC_PARITY = 0x24,
    
    // Custom types (0x80-0xFF)
    CUSTOM_START = 0x80
};

/**
 * @brief Packet flags for additional information
 */
enum class PacketFlags : uint8_t {
    NONE = 0x00,
    RELIABLE = 0x01,      // Requires acknowledgment
    FRAGMENTED = 0x02,    // Packet is fragmented
    COMPRESSED = 0x04,    // Data is compressed
    ENCRYPTED = 0x08,     // Data is encrypted
    SEQUENCED = 0x10,     // Sequenced delivery
    ORDERED = 0x20        // Ordered delivery
};

/**
 * @brief Packet header structure
 */
struct PacketHeader {
    uint8_t version;          // Protocol version
    PacketType type;          // Packet type
    PacketFlags flags;        // Packet flags
    uint32_t sequence;        // Sequence number
    uint32_t timestamp;       // Timestamp
    uint32_t dataSize;        // Size of data payload
    uint32_t checksum;        // Data checksum
    // FEC metadata (optional)
    uint16_t fecGroupId;      // Group identifier
    uint8_t fecIndex;         // Index within group (0..N-1), 255 for parity
    uint8_t fecGroupSize;     // Total data packets in group
    // Fragment metadata (optional)
    uint16_t fragGroupId;     // Group ID for fragmentation (usually sequence low bits)
    uint16_t fragIndex;       // Fragment index
    uint16_t fragCount;       // Total fragments
    
    PacketHeader() : version(1), type(PacketType::DATA), flags(PacketFlags::NONE),
                    sequence(0), timestamp(0), dataSize(0), checksum(0),
                    fecGroupId(0), fecIndex(0), fecGroupSize(0),
                    fragGroupId(0), fragIndex(0), fragCount(0) {}
};

/**
 * @brief Network packet class
 */
class Packet {
public:
    Packet();
    Packet(PacketType type, const std::vector<uint8_t>& data);
    Packet(PacketType type, const void* data, size_t size);
    
    // Copy constructor and assignment
    Packet(const Packet& other);
    Packet& operator=(const Packet& other);
    
    // Move constructor and assignment
    Packet(Packet&& other) noexcept;
    Packet& operator=(Packet&& other) noexcept;
    
    /**
     * @brief Set packet type
     * @param type Packet type
     */
    void setType(PacketType type);
    
    /**
     * @brief Get packet type
     * @return Packet type
     */
    PacketType getType() const;
    
    /**
     * @brief Set packet flags
     * @param flags Packet flags
     */
    void setFlags(PacketFlags flags);
    
    /**
     * @brief Get packet flags
     * @return Packet flags
     */
    PacketFlags getFlags() const;
    
    /**
     * @brief Add flag
     * @param flag Flag to add
     */
    void addFlag(PacketFlags flag);
    
    /**
     * @brief Remove flag
     * @param flag Flag to remove
     */
    void removeFlag(PacketFlags flag);
    
    /**
     * @brief Check if flag is set
     * @param flag Flag to check
     * @return true if flag is set
     */
    bool hasFlag(PacketFlags flag) const;
    
    /**
     * @brief Set sequence number
     * @param sequence Sequence number
     */
    void setSequence(uint32_t sequence);
    
    /**
     * @brief Get sequence number
     * @return Sequence number
     */
    uint32_t getSequence() const;
    
    /**
     * @brief Set packet data
     * @param data Data to set
     */
    void setData(const std::vector<uint8_t>& data);
    
    /**
     * @brief Set packet data
     * @param data Data pointer
     * @param size Data size
     */
    void setData(const void* data, size_t size);
    
    /**
     * @brief Get packet data
     * @return Reference to data vector
     */
    const std::vector<uint8_t>& getData() const;
    
    /**
     * @brief Get packet data size
     * @return Data size in bytes
     */
    size_t getDataSize() const;
    
    /**
     * @brief Get total packet size (header + data)
     * @return Total size in bytes
     */
    size_t getTotalSize() const;
    
    /**
     * @brief Serialize packet to byte array
     * @return Serialized packet data
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * @brief Deserialize packet from byte array
     * @param data Serialized packet data
     * @return true if successful, false otherwise
     */
    bool deserialize(const std::vector<uint8_t>& data);
    
    /**
     * @brief Deserialize packet from byte array
     * @param data Data pointer
     * @param size Data size
     * @return true if successful, false otherwise
     */
    bool deserialize(const void* data, size_t size);
    
    /**
     * @brief Calculate packet checksum
     * @return Checksum value
     */
    uint32_t calculateChecksum() const;
    
    /**
     * @brief Verify packet checksum
     * @return true if checksum is valid
     */
    bool verifyChecksum() const;
    
    /**
     * @brief Update timestamp to current time
     */
    void updateTimestamp();
    
    /**
     * @brief Get packet timestamp
     * @return Timestamp
     */
    uint32_t getTimestamp() const;
    
    // FEC metadata
    void setFECGroup(uint16_t groupId, uint8_t index, uint8_t groupSize);
    uint16_t getFECGroupId() const { return m_header.fecGroupId; }
    uint8_t getFECIndex() const { return m_header.fecIndex; }
    uint8_t getFECGroupSize() const { return m_header.fecGroupSize; }

    // Fragment metadata
    void setFragment(uint16_t groupId, uint16_t index, uint16_t count);
    uint16_t getFragGroupId() const { return m_header.fragGroupId; }
    uint16_t getFragIndex() const { return m_header.fragIndex; }
    uint16_t getFragCount() const { return m_header.fragCount; }
    
    /**
     * @brief Check if packet is expired
     * @param maxAge Maximum age in milliseconds
     * @return true if packet is expired
     */
    bool isExpired(uint32_t maxAge = 5000) const;
    
    /**
     * @brief Clear packet data
     */
    void clear();
    
    /**
     * @brief Check if packet is empty
     * @return true if packet has no data
     */
    bool isEmpty() const;

private:
    PacketHeader m_header;
    std::vector<uint8_t> m_data;
    
    void updateHeader();
    uint32_t getCurrentTimestamp() const;
};

/**
 * @brief Packet builder for easy packet creation
 */
class PacketBuilder {
public:
    PacketBuilder();
    
    /**
     * @brief Set packet type
     * @param type Packet type
     * @return Reference to builder
     */
    PacketBuilder& type(PacketType type);
    
    /**
     * @brief Set packet flags
     * @param flags Packet flags
     * @return Reference to builder
     */
    PacketBuilder& flags(PacketFlags flags);
    
    /**
     * @brief Add flag
     * @param flag Flag to add
     * @return Reference to builder
     */
    PacketBuilder& addFlag(PacketFlags flag);
    
    /**
     * @brief Set sequence number
     * @param sequence Sequence number
     * @return Reference to builder
     */
    PacketBuilder& sequence(uint32_t sequence);
    
    /**
     * @brief Set packet data
     * @param data Data to set
     * @return Reference to builder
     */
    PacketBuilder& data(const std::vector<uint8_t>& data);
    
    /**
     * @brief Set packet data
     * @param data Data pointer
     * @param size Data size
     * @return Reference to builder
     */
    PacketBuilder& data(const void* data, size_t size);
    
    /**
     * @brief Build the packet
     * @return Built packet
     */
    Packet build();

private:
    Packet m_packet;
};

} // namespace core
} // namespace tienet
