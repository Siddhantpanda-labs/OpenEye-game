/**
 * @file packet.cpp
 * @brief Generic packet system implementation
 */

#include "tienet/core/packet.hpp"
#include <cstring>
#include <algorithm>

namespace tienet {
namespace core {

// Packet implementation
Packet::Packet() {
    updateHeader();
}

Packet::Packet(PacketType type, const std::vector<uint8_t>& data) 
    : m_data(data) {
    m_header.type = type;
    updateHeader();
}

Packet::Packet(PacketType type, const void* data, size_t size) {
    m_data.resize(size);
    std::memcpy(m_data.data(), data, size);
    m_header.type = type;
    updateHeader();
}

Packet::Packet(const Packet& other) 
    : m_header(other.m_header), m_data(other.m_data) {
}

Packet& Packet::operator=(const Packet& other) {
    if (this != &other) {
        m_header = other.m_header;
        m_data = other.m_data;
    }
    return *this;
}

Packet::Packet(Packet&& other) noexcept 
    : m_header(std::move(other.m_header)), m_data(std::move(other.m_data)) {
    other.m_header = PacketHeader();
    other.m_data.clear();
}

Packet& Packet::operator=(Packet&& other) noexcept {
    if (this != &other) {
        m_header = std::move(other.m_header);
        m_data = std::move(other.m_data);
        other.m_header = PacketHeader();
        other.m_data.clear();
    }
    return *this;
}

void Packet::setType(PacketType type) {
    m_header.type = type;
    updateHeader();
}

PacketType Packet::getType() const {
    return m_header.type;
}

void Packet::setFlags(PacketFlags flags) {
    m_header.flags = flags;
    updateHeader();
}

PacketFlags Packet::getFlags() const {
    return m_header.flags;
}

void Packet::addFlag(PacketFlags flag) {
    m_header.flags = static_cast<PacketFlags>(static_cast<uint8_t>(m_header.flags) | static_cast<uint8_t>(flag));
    updateHeader();
}

void Packet::removeFlag(PacketFlags flag) {
    m_header.flags = static_cast<PacketFlags>(static_cast<uint8_t>(m_header.flags) & ~static_cast<uint8_t>(flag));
    updateHeader();
}

bool Packet::hasFlag(PacketFlags flag) const {
    return (static_cast<uint8_t>(m_header.flags) & static_cast<uint8_t>(flag)) != 0;
}

void Packet::setSequence(uint32_t sequence) {
    m_header.sequence = sequence;
    updateHeader();
}

uint32_t Packet::getSequence() const {
    return m_header.sequence;
}

void Packet::setData(const std::vector<uint8_t>& data) {
    m_data = data;
    updateHeader();
}

void Packet::setData(const void* data, size_t size) {
    m_data.resize(size);
    std::memcpy(m_data.data(), data, size);
    updateHeader();
}

const std::vector<uint8_t>& Packet::getData() const {
    return m_data;
}

size_t Packet::getDataSize() const {
    return m_data.size();
}

size_t Packet::getTotalSize() const {
    return sizeof(PacketHeader) + m_data.size();
}

std::vector<uint8_t> Packet::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(getTotalSize());
    
    // Serialize header
    const uint8_t* headerPtr = reinterpret_cast<const uint8_t*>(&m_header);
    result.insert(result.end(), headerPtr, headerPtr + sizeof(PacketHeader));
    
    // Serialize data
    result.insert(result.end(), m_data.begin(), m_data.end());
    
    return result;
}

bool Packet::deserialize(const std::vector<uint8_t>& data) {
    return deserialize(data.data(), data.size());
}

bool Packet::deserialize(const void* data, size_t size) {
    if (size < sizeof(PacketHeader)) {
        return false;
    }
    
    // Deserialize header
    std::memcpy(&m_header, data, sizeof(PacketHeader));
    
    // Check data size
    if (size < sizeof(PacketHeader) + m_header.dataSize) {
        return false;
    }
    
    // Deserialize data
    m_data.resize(m_header.dataSize);
    const uint8_t* dataPtr = static_cast<const uint8_t*>(data) + sizeof(PacketHeader);
    std::memcpy(m_data.data(), dataPtr, m_header.dataSize);
    
    return true;
}

uint32_t Packet::calculateChecksum() const {
    uint32_t checksum = 0;
    
    // Calculate checksum for data only
    const uint8_t* dataPtr = m_data.data();
    size_t dataSize = m_data.size();
    
    for (size_t i = 0; i < dataSize; ++i) {
        checksum += dataPtr[i];
    }
    
    return checksum;
}

bool Packet::verifyChecksum() const {
    return m_header.checksum == calculateChecksum();
}

void Packet::updateTimestamp() {
    m_header.timestamp = getCurrentTimestamp();
    updateHeader();
}

uint32_t Packet::getTimestamp() const {
    return m_header.timestamp;
}

void Packet::setFECGroup(uint16_t groupId, uint8_t index, uint8_t groupSize) {
    m_header.fecGroupId = groupId;
    m_header.fecIndex = index;
    m_header.fecGroupSize = groupSize;
    updateHeader();
}

void Packet::setFragment(uint16_t groupId, uint16_t index, uint16_t count) {
    m_header.fragGroupId = groupId;
    m_header.fragIndex = index;
    m_header.fragCount = count;
    updateHeader();
}

bool Packet::isExpired(uint32_t maxAge) const {
    uint32_t currentTime = getCurrentTimestamp();
    uint32_t age = currentTime - m_header.timestamp;
    return age > maxAge;
}

void Packet::clear() {
    m_header = PacketHeader();
    m_data.clear();
}

bool Packet::isEmpty() const {
    return m_data.empty();
}

void Packet::updateHeader() {
    m_header.dataSize = static_cast<uint32_t>(m_data.size());
    m_header.checksum = calculateChecksum();
    if (m_header.timestamp == 0) {
        m_header.timestamp = getCurrentTimestamp();
    }
}

uint32_t Packet::getCurrentTimestamp() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
}

// PacketBuilder implementation
PacketBuilder::PacketBuilder() {
    m_packet = Packet();
}

PacketBuilder& PacketBuilder::type(PacketType type) {
    m_packet.setType(type);
    return *this;
}

PacketBuilder& PacketBuilder::flags(PacketFlags flags) {
    m_packet.setFlags(flags);
    return *this;
}

PacketBuilder& PacketBuilder::addFlag(PacketFlags flag) {
    m_packet.addFlag(flag);
    return *this;
}

PacketBuilder& PacketBuilder::sequence(uint32_t sequence) {
    m_packet.setSequence(sequence);
    return *this;
}

PacketBuilder& PacketBuilder::data(const std::vector<uint8_t>& data) {
    m_packet.setData(data);
    return *this;
}

PacketBuilder& PacketBuilder::data(const void* data, size_t size) {
    m_packet.setData(data, size);
    return *this;
}

Packet PacketBuilder::build() {
    return std::move(m_packet);
}

} // namespace core
} // namespace tienet
