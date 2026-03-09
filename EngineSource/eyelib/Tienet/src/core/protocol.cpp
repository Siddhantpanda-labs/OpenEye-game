/**
 * @file protocol.cpp
 * @brief Generic protocol handler implementation
 */

#include "tienet/core/protocol.hpp"
#include "tienet/core/pacer.hpp"
#include "tienet/core/rate_control.hpp"
#include "tienet/core/fec.hpp"
#include "tienet/core/security.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>

namespace tienet {
namespace core {

// Protocol implementation
Protocol::Protocol() 
    : m_state(ProtocolState::Disconnected), m_nextSequence(1), m_lastAckSequence(0),
      m_currentMTU(1200), m_mtuProbePending(false), m_duplicateAcks(0), m_lastAckedSequence(0) {
}

bool Protocol::initialize(const ProtocolConfig& config) {
    m_config = config;
    m_state = ProtocolState::Disconnected;
    m_nextSequence = 1;
    m_lastAckSequence = 0;
    
    // Initialize MTU discovery
    m_currentMTU = m_config.conservativeMTU;
    m_mtuProbePending = false;
    m_duplicateAcks = 0;
    m_lastAckedSequence = 0;
    
    m_fragments.clear();
    m_fragmentTimestamps.clear();
    m_sentPackets.clear();
    m_retryCount.clear();
    m_sendQueue.clear();
    m_fecGroup.clear();
    m_sendBuffer.clear();

    // Initialize crypto
    if (m_config.enableEncryption) {
        m_cipher = CryptoFactory::create(m_cryptoConfig);
        if (m_cipher) m_cipher->initialize(m_cryptoConfig);
    } else {
        m_cipher.reset();
    }
    
    if (m_config.enablePacing) {
        m_pacer = std::make_unique<Pacer>();
        m_pacer->setTargetBitrate(m_config.initialTargetBitrateBps);
    } else {
        m_pacer.reset();
    }
    if (m_config.enableAdaptiveBitrate) {
        m_rateController = std::make_unique<RateController>();
        m_rateController->setBounds(m_config.minBitrateBps, m_config.maxBitrateBps);
        m_rateController->setInitial(m_config.initialTargetBitrateBps);
    } else {
        m_rateController.reset();
    }
    
    return true;
}

void Protocol::shutdown() {
    m_state = ProtocolState::Disconnected;
    m_socket.reset();
    m_fragments.clear();
    m_fragmentTimestamps.clear();
    m_sentPackets.clear();
    m_retryCount.clear();
    m_sendBuffer.clear();
}

void Protocol::setSocket(std::shared_ptr<Socket> socket) {
    m_socket = socket;
}

bool Protocol::connect(const SocketAddress& address) {
    if (!m_socket) {
        return false;
    }
    
    m_state = ProtocolState::Connecting;
    
    if (!m_socket->connect(address)) {
        m_state = ProtocolState::Error;
        handleError("Failed to connect to " + address.toString());
        return false;
    }
    
    m_state = ProtocolState::Connected;
    handleConnection(true);
    
    return true;
}

void Protocol::disconnect() {
    if (m_socket) {
        m_socket->close();
    }
    
    m_state = ProtocolState::Disconnected;
    handleConnection(false);
}

bool Protocol::sendPacket(const Packet& packet) {
    if (!m_socket || m_state != ProtocolState::Connected) {
        return false;
    }
    
    // Serialize packet
    std::vector<uint8_t> data = packet.serialize();
    // Encrypt payload when enabled (AAD: header)
    if (m_cipher && m_config.enableEncryption && !data.empty()) {
        if (data.size() >= sizeof(PacketHeader)) {
            const uint8_t* aad = data.data();
            size_t aadLen = sizeof(PacketHeader);
            std::vector<uint8_t> encrypted;
            uint64_t nonce = static_cast<uint64_t>(packet.getSequence());
            if (m_cipher->encrypt(data.data() + aadLen, data.size() - aadLen, encrypted, aad, aadLen, nonce)) {
                // Rebuild packet bytes: header + ciphertext
                data.resize(aadLen);
                data.insert(data.end(), encrypted.begin(), encrypted.end());
                // Mark as encrypted
                // Note: flags are inside header; for simplicity we rely on sender setting PacketFlags::ENCRYPTED
            }
        }
    }
    
    // Add to send buffer if reliable
    if (isPacketReliable(packet)) {
        addToSendBuffer(packet.getSequence(), data, true, packet.getType());
    }
    
    if (m_config.enablePacing) {
        return enqueueDatagram(data);
    } else {
        int sent = m_socket->send(data.data(), data.size());
        if (sent == -1) {
            handleError("Failed to send packet");
            return false;
        }
        m_stats.packetsSent++;
        m_stats.bytesSent += sent;
    }
    
    return true;
}

bool Protocol::sendData(const void* data, size_t size, PacketType type) {
    Packet packet(type, data, size);
    return sendPacket(packet);
}

bool Protocol::sendLarge(const void* data, size_t size, PacketType type) {
    // Compute safe payload MTU using current MTU
    const size_t headerSize = sizeof(PacketHeader);
    size_t maxPayload = (m_currentMTU > headerSize + 50 ? m_currentMTU - headerSize - 50 : 1200);
    if (size <= maxPayload) {
        return sendData(data, size, type);
    }
    // Fragment
    uint16_t fragCount = static_cast<uint16_t>((size + maxPayload - 1) / maxPayload);
    uint16_t groupId = static_cast<uint16_t>(getNextSequence() & 0xFFFF);
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (uint16_t i = 0; i < fragCount; ++i) {
        size_t offset = static_cast<size_t>(i) * maxPayload;
        size_t chunk = std::min(maxPayload, size - offset);
        Packet p(type, bytes + offset, chunk);
        p.setFragment(groupId, i, fragCount);
        if (!sendPacket(p)) return false;
    }
    return true;
}

bool SimpleUDPProtocol::sendLargeTo(const void* data, size_t size, PacketType type, const SocketAddress& address) {
    const size_t headerSize = sizeof(PacketHeader);
    size_t maxPayload = (m_config.maxPacketSize > headerSize + 50 ? m_config.maxPacketSize - headerSize - 50 : 1200);
    if (size <= maxPayload) {
        Packet p(type, data, size);
        return sendPacketTo(p, address);
    }
    uint16_t fragCount = static_cast<uint16_t>((size + maxPayload - 1) / maxPayload);
    uint16_t groupId = static_cast<uint16_t>(getNextSequence() & 0xFFFF);
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (uint16_t i = 0; i < fragCount; ++i) {
        size_t offset = static_cast<size_t>(i) * maxPayload;
        size_t chunk = std::min(maxPayload, size - offset);
        Packet p(type, bytes + offset, chunk);
        p.setFragment(groupId, i, fragCount);
        if (!sendPacketTo(p, address)) return false;
    }
    return true;
}

bool Protocol::processData(const void* data, size_t size) {
    if (!data || size == 0) {
        return false;
    }
    
    // Try to deserialize packet
    Packet packet;
    if (!packet.deserialize(data, size)) {
        return false;
    }
    
    // Verify checksum
    if (!packet.verifyChecksum()) {
        m_stats.packetsDropped++;
        return false;
    }
    
    // Fragment reassembly path using new header fields
    if (packet.getFragCount() > 0) {
        uint32_t key = (static_cast<uint32_t>(packet.getFragGroupId()) << 16) | packet.getFragCount();
        // Create or append vector for this group
        auto& vec = m_fragments[key];
        if (vec.empty()) vec.resize(packet.getFragCount());
        uint16_t idx = packet.getFragIndex();
        if (idx < vec.size()) vec[idx] = std::make_unique<Packet>(packet);
        // Check completeness
        bool complete = true;
        for (const auto& e : vec) { if (!e) { complete = false; break; } }
        if (!complete) return true;
        // Reassemble by concatenating payloads
        std::vector<uint8_t> reassembled;
        for (const auto& e : vec) {
            const auto& d = e->getData();
            reassembled.insert(reassembled.end(), d.begin(), d.end());
        }
        Packet full(packet.getType(), reassembled);
        handlePacket(full);
        m_fragments.erase(key);
        return true;
    }
    
    // Update statistics
    m_stats.packetsReceived++;
    m_stats.bytesReceived += size;
    
    // Decrypt if needed
    if (m_cipher && packet.hasFlag(PacketFlags::ENCRYPTED)) {
        // Parse serialized again to get raw bytes
        std::vector<uint8_t> bytes(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
        if (bytes.size() >= sizeof(PacketHeader)) {
            const uint8_t* aad = bytes.data();
            size_t aadLen = sizeof(PacketHeader);
            std::vector<uint8_t> decrypted;
            uint64_t nonce = static_cast<uint64_t>(packet.getSequence());
            if (m_cipher->decrypt(bytes.data() + aadLen, bytes.size() - aadLen, decrypted, aad, aadLen, nonce)) {
                Packet plain(packet.getType(), decrypted);
                plain.setSequence(packet.getSequence());
                plain.setFlags(packet.getFlags());
                handlePacket(plain);
                return true;
            }
        }
        // On failure, drop
        return false;
    } else {
        // Handle packet
        handlePacket(packet);
    }
    
    return true;
}

void Protocol::update() {
    if (!m_socket) {
        return;
    }
    
    // Cleanup expired fragments and packets
    cleanupExpiredFragments();
    cleanupExpiredPackets();
    
    // Check for lost packets and retransmit
    checkForLostPackets();
    
    // Perform MTU probe if enabled and needed
    if (m_config.enablePMTUDiscovery && !m_mtuProbePending) {
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastProbe = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastMTUProbe);
        if (timeSinceLastProbe.count() > 30000) { // Probe every 30 seconds
            performMTUProbe();
        }
    }
    
    applyRateUpdate();
    drainSendQueue();
    
    // Process incoming data
    std::vector<uint8_t> buffer(65536);
    int received = m_socket->receive(buffer.data(), buffer.size());
    
    if (received > 0) {
        processData(buffer.data(), received);
    } else if (received == -1) {
        // Error occurred
        m_state = ProtocolState::Error;
        handleError("Socket error occurred");
    }
}

ProtocolState Protocol::getState() const {
    return m_state;
}

ProtocolStats Protocol::getStats() const {
    return m_stats;
}

void Protocol::resetStats() {
    m_stats = ProtocolStats();
}

void Protocol::setEventCallback(EventCallback callback) {
    m_eventCallback = callback;
}

void Protocol::setDataCallback(DataCallback callback) {
    m_dataCallback = callback;
}

void Protocol::setConfig(const ProtocolConfig& config) {
    m_config = config;
}

ProtocolConfig Protocol::getConfig() const {
    return m_config;
}

void Protocol::handlePacket(const Packet& packet) {
    // Handle different packet types
    switch (packet.getType()) {
        case PacketType::ACK:
            // Handle acknowledgment and update RTT
            if (m_sentPackets.find(packet.getSequence()) != m_sentPackets.end()) {
                auto sentTime = m_sentPackets[packet.getSequence()];
                auto now = std::chrono::steady_clock::now();
                auto rttMs = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - sentTime).count());
                updateLatency(rttMs);
                removeFromSendBuffer(packet.getSequence());
                m_sentPackets.erase(packet.getSequence());
                m_retryCount.erase(packet.getSequence());
            }
            // Optional SACK parsing: 8-byte payload (ackBase|mask)
            if (packet.getDataSize() >= 8) {
                uint32_t ackBase = 0, mask = 0;
                std::memcpy(&ackBase, packet.getData().data(), 4);
                std::memcpy(&mask, packet.getData().data() + 4, 4);
                for (auto it = m_sentPackets.begin(); it != m_sentPackets.end();) {
                    uint32_t seq = it->first;
                    bool acked = false;
                    if (seq <= ackBase) acked = true;
                    else if (seq > ackBase && seq <= ackBase + 32) {
                        uint32_t bit = seq - ackBase - 1;
                        acked = ((mask >> bit) & 1u) != 0u;
                    }
                    if (acked) {
                        removeFromSendBuffer(seq);
                        m_retryCount.erase(seq);
                        it = m_sentPackets.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            // Handle fast retransmit
            handleFastRetransmit(packet.getSequence());
            break;
        case PacketType::NACK:
            // Handle negative acknowledgment
            break;
        case PacketType::FRAGMENT:
            // Handle fragment
            processFragment(packet);
            break;
        case PacketType::PING:
            // Handle ping
            if (packet.hasFlag(PacketFlags::RELIABLE)) {
                updateRecvWindow(packet.getSequence());
                sendAck(packet.getSequence());
            }
            break;
        case PacketType::PONG:
            // Handle pong
            break;
        default:
            // Handle regular data packet
            if (packet.hasFlag(PacketFlags::RELIABLE)) {
                updateRecvWindow(packet.getSequence());
                sendAck(packet.getSequence());
            }
            
            if (m_dataCallback) {
                m_dataCallback(packet);
            }
            break;
    }
}

void Protocol::updateRecvWindow(uint32_t sequence) {
    if (sequence == 0) return;
    if (m_recvAckBase == 0) {
        m_recvAckBase = sequence;
        m_recvHighestSeq = sequence;
        m_recvWindowMask = 0;
        return;
    }
    if (sequence <= m_recvAckBase) {
        // Already acked contiguously or old
        return;
    }
    if (sequence == m_recvAckBase + 1) {
        m_recvAckBase = sequence;
        // Shift mask down; the next expected advances
        m_recvWindowMask >>= 1;
    } else {
        uint32_t delta = sequence - m_recvAckBase - 1;
        if (delta < 32) {
            m_recvWindowMask |= (1u << delta);
            if (sequence > m_recvHighestSeq) m_recvHighestSeq = sequence;
        } else {
            // Jump too far; reset window to this sequence
            m_recvAckBase = sequence;
            m_recvWindowMask = 0;
            m_recvHighestSeq = sequence;
        }
    }
}

void Protocol::handleConnection(bool connected) {
    if (m_eventCallback) {
        ProtocolEvent event = connected ? ProtocolEvent::Connected : ProtocolEvent::Disconnected;
        m_eventCallback(event, Packet());
    }
}

void Protocol::handleError(const std::string& error) {
    m_state = ProtocolState::Error;
    
    if (m_eventCallback) {
        m_eventCallback(ProtocolEvent::Error, Packet());
    }
    
    std::cerr << "Protocol error: " << error << std::endl;
}

void Protocol::sendAck(uint32_t sequence) {
    uint32_t ackBase = m_recvAckBase;
    uint32_t mask = m_recvWindowMask;
    std::vector<uint8_t> payload(8);
    std::memcpy(payload.data(), &ackBase, 4);
    std::memcpy(payload.data() + 4, &mask, 4);
    Packet ackPacket(PacketType::ACK, payload);
    ackPacket.setSequence(sequence);
    sendPacket(ackPacket);
}

void Protocol::sendNack(uint32_t sequence) {
    Packet nackPacket = PacketBuilder()
        .type(PacketType::NACK)
        .sequence(sequence)
        .build();
    
    sendPacket(nackPacket);
}

void Protocol::processFragment(const Packet& packet) {
    uint32_t sequence = packet.getSequence();
    
    // Add fragment to reassembly buffer
    if (m_fragments.find(sequence) == m_fragments.end()) {
        m_fragments[sequence] = std::vector<std::unique_ptr<Packet>>();
        m_fragmentTimestamps[sequence] = std::chrono::steady_clock::now();
    }
    
    m_fragments[sequence].push_back(std::make_unique<Packet>(packet));
    
    // Try to reassemble
    auto reassembled = reassemblePacket(sequence);
    if (reassembled) {
        handlePacket(*reassembled);
        m_fragments.erase(sequence);
        m_fragmentTimestamps.erase(sequence);
    }
}

std::unique_ptr<Packet> Protocol::reassemblePacket(uint32_t sequence) {
    auto it = m_fragments.find(sequence);
    if (it == m_fragments.end()) {
        return nullptr;
    }
    
    const auto& fragments = it->second;
    if (fragments.empty()) {
        return nullptr;
    }
    
    // Check if we have all fragments
    // This is a simplified implementation - in practice, you'd need
    // to track fragment numbers and total fragment count
    
    // For now, just return the first fragment
    return std::make_unique<Packet>(*fragments[0]);
}

void Protocol::cleanupExpiredFragments() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = m_fragmentTimestamps.begin(); it != m_fragmentTimestamps.end();) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
        if (duration.count() > m_config.fragmentTimeout) {
            m_fragments.erase(it->first);
            it = m_fragmentTimestamps.erase(it);
        } else {
            ++it;
        }
    }
}

void Protocol::cleanupExpiredPackets() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = m_sentPackets.begin(); it != m_sentPackets.end();) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);
        if (duration.count() > m_config.ackTimeout) {
            // Retry packet
            uint32_t sequence = it->first;
            auto retryIt = m_retryCount.find(sequence);
            if (retryIt != m_retryCount.end() && retryIt->second < m_config.maxRetries) {
                retryIt->second++;
                it->second = now; // Reset timestamp
                ++it;
            } else {
                // Give up on this packet
                m_stats.packetsLost++;
                m_retryCount.erase(sequence);
                it = m_sentPackets.erase(it);
            }
        } else {
            ++it;
        }
    }
}

void Protocol::updateLatency(uint32_t latency) {
    m_stats.averageLatency = (m_stats.averageLatency + latency) / 2;
    m_stats.maxLatency = std::max(m_stats.maxLatency, latency);
    m_stats.minLatency = std::min(m_stats.minLatency, latency);
}

uint32_t Protocol::getNextSequence() {
    return m_nextSequence++;
}

bool Protocol::isPacketReliable(const Packet& packet) const {
    return packet.hasFlag(PacketFlags::RELIABLE);
}

bool Protocol::enqueueDatagram(const std::vector<uint8_t>& bytes) {
    if (!m_config.enablePacing || !m_pacer) {
        return false;
    }
    m_sendQueue.push_back(bytes);
    return true;
}

void Protocol::applyRateUpdate() {
    if (!m_rateController) return;
    // Build sample from current stats; acks/loss should update these in real time
    RateSample s;
    s.rttMs = m_stats.averageLatency;
    // Approximate loss from packetsLost over packetsSent in last window (simplified)
    float sent = static_cast<float>(std::max<uint64_t>(m_stats.packetsSent, 1));
    s.lossPercent = (static_cast<float>(m_stats.packetsLost) / sent) * 100.0f;
    s.bytesAcked = m_stats.bytesReceived; // placeholder: track acked bytes separately if available
    s.windowMs = 1000;
    uint32_t target = m_rateController->update(s);
    m_targetBitrateBps = target;
    if (m_pacer) m_pacer->setTargetBitrate(target);
}
uint32_t Protocol::getTargetBitrateBps() const {
    return m_targetBitrateBps ? m_targetBitrateBps : m_config.initialTargetBitrateBps;
}

void Protocol::drainSendQueue() {
    if (!m_config.enablePacing || !m_pacer) return;
    uint32_t budget = m_pacer->dequeueBudgetBytes();
    while (budget > 0 && !m_sendQueue.empty()) {
        std::vector<uint8_t> bytes = std::move(m_sendQueue.front());
        if (bytes.size() > budget) break; // wait for more tokens
        m_sendQueue.pop_front();
        int sent = m_socket->send(bytes.data(), bytes.size());
        if (sent > 0) {
            m_stats.packetsSent++;
            m_stats.bytesSent += sent;
            budget -= static_cast<uint32_t>(sent);
        } else {
            // On error, stop draining to avoid spin
            break;
        }
    }
}

// SimpleUDPProtocol implementation
SimpleUDPProtocol::SimpleUDPProtocol() : Protocol() {
}

bool SimpleUDPProtocol::sendPacketTo(const Packet& packet, const SocketAddress& address) {
    if (!m_socket || m_state != ProtocolState::Connected) {
        return false;
    }
    
    // Serialize packet
    std::vector<uint8_t> data = packet.serialize();
    if (m_config.enablePacing || (m_config.enableFEC && m_config.fecGroupSize > 1)) {
        // Store with destination for later draining; accumulate for optional FEC
        if (m_config.enableFEC && m_config.fecGroupSize > 1) {
            if (!m_fecGroupActive || address.host != m_fecGroupAddr.host || address.port != m_fecGroupAddr.port) {
                // reset group for new address
                m_fecGroupTo.clear();
                m_fecGroupAddr = address;
                m_fecGroupActive = true;
                m_fecGroupFirstSeq = packet.getSequence();
            }
            m_fecGroupTo.push_back({ data, address });
            if (m_fecGroupTo.size() >= m_config.fecGroupSize) {
                // Interleave across stride if configured (simple round-robin across current group)
                // Build parity packets (k times) - using same XOR for simplicity
                std::vector<std::vector<uint8_t>> payloads;
                payloads.reserve(m_fecGroupTo.size());
                for (auto& o : m_fecGroupTo) payloads.push_back(o.bytes);
                // Enqueue group packets
                for (auto& o : m_fecGroupTo) m_outQueue.push_back(std::move(o));
                for (uint32_t pi = 0; pi < std::max(1u, m_config.fecParityCount); ++pi) {
                    auto parity = FECEncoder::xorParity(payloads);
                    Packet parityPkt(PacketType::FEC_PARITY, parity);
                    parityPkt.setFECGroup(static_cast<uint16_t>(m_fecGroupFirstSeq & 0xFFFF), 255, static_cast<uint8_t>(m_config.fecGroupSize));
                    auto parityBytes = parityPkt.serialize();
                    m_outQueue.push_back({ std::move(parityBytes), address });
                }
                m_fecGroupTo.clear();
                m_fecGroupActive = false;
            }
        } else {
            m_outQueue.push_back({ std::move(data), address });
        }
        return true;
    } else {
        int sent = m_socket->sendTo(data.data(), data.size(), address);
        if (sent == -1) {
            handleError("Failed to send packet to " + address.toString());
            return false;
        }
        m_stats.packetsSent++;
        m_stats.bytesSent += sent;
    }
    
    return true;
}

// Ensure base provides default implementation (already added in header)

bool SimpleUDPProtocol::processDataFrom(const void* data, size_t size, const SocketAddress& address) {
    // We could track per-address statistics here if needed
    return processData(data, size);
}

void SimpleUDPProtocol::handlePacket(const Packet& packet) {
    // For UDP, we don't ack everything; only reliable packets
    if (packet.hasFlag(PacketFlags::RELIABLE)) {
        sendAck(packet.getSequence());
    }

    // FEC receive path: collect data and parity, attempt single-loss recovery
    if (m_config.enableFEC && packet.getType() != PacketType::ACK && packet.getType() != PacketType::NACK) {
        uint16_t gid = packet.getFECGroupId();
        uint8_t gsz = packet.getFECGroupSize();
        uint8_t idx = packet.getFECIndex();

        if (gsz > 0) {
            auto& group = m_fecRecvGroups[gid];
            if (group.groupSize == 0) {
                group.groupSize = gsz;
                group.present.assign(gsz, false);
                group.data.assign(gsz, {});
                group.parities.clear();
            }
            // Store packet bytes
            auto bytes = packet.serialize();
            if (packet.getType() == PacketType::FEC_PARITY) {
                group.hasParity = true;
                group.parities.push_back(std::move(bytes));
            } else if (idx < group.groupSize) {
                group.present[idx] = true;
                group.data[idx] = std::move(bytes);
            }

            // Count missing
            size_t missing = 0; size_t missingIndex = 0;
            for (size_t i = 0; i < group.groupSize; ++i) {
                if (!group.present[i]) { missing++; missingIndex = i; }
            }

            // If complete, or single missing with parity available
            if (missing == 0) {
                // Deliver all data packets
                for (size_t i = 0; i < group.groupSize; ++i) {
                    Packet p;
                    if (p.deserialize(group.data[i])) {
                        if (m_dataCallback && p.getType() != PacketType::FEC_PARITY) m_dataCallback(p);
                    }
                }
                m_fecRecvGroups.erase(gid);
                return;
            } else if (missing == 1 && group.hasParity) {
                // Recover missing using XOR parity on payloads
                std::vector<std::vector<uint8_t>> payloads;
                payloads.reserve(group.groupSize - 1);
                for (size_t i = 0; i < group.groupSize; ++i) {
                    if (!group.present[i]) continue;
                    // Extract payload from serialized packet
                    if (group.data[i].size() >= sizeof(PacketHeader)) {
                        payloads.emplace_back(group.data[i].begin() + sizeof(PacketHeader), group.data[i].end());
                    }
                }
                // Try any available parity
                std::vector<uint8_t> recoveredPayload;
                bool recoveredOk = false;
                for (const auto& parityBytes : group.parities) {
                    std::vector<uint8_t> parityPayload;
                    if (parityBytes.size() >= sizeof(PacketHeader)) {
                        parityPayload.assign(parityBytes.begin() + sizeof(PacketHeader), parityBytes.end());
                    }
                    if (FECDecoder::recoverXor(payloads, parityPayload, group.groupSize, recoveredPayload)) { recoveredOk = true; break; }
                }
                if (recoveredOk) {
                    // Build recovered packet (use first present header as template)
                    Packet templatePkt;
                    for (size_t i = 0; i < group.groupSize; ++i) {
                        if (group.present[i]) { templatePkt.deserialize(group.data[i]); break; }
                    }
                    Packet recovered(templatePkt.getType(), recoveredPayload);
                    if (m_dataCallback) m_dataCallback(recovered);
                }
                // Deliver all present packets too
                for (size_t i = 0; i < group.groupSize; ++i) {
                    if (!group.present[i]) continue;
                    Packet p;
                    if (p.deserialize(group.data[i])) {
                        if (m_dataCallback && p.getType() != PacketType::FEC_PARITY) m_dataCallback(p);
                    }
                }
                m_fecRecvGroups.erase(gid);
                return;
            }
            // Not complete yet; wait for more
            return;
        }
    }

    // Default: forward packet
    if (m_dataCallback) {
        m_dataCallback(packet);
    }
}

bool SimpleUDPProtocol::sendPacket(const Packet& packet) {
    // Not used for connectionless UDP without a bound peer
    return false;
}

bool SimpleUDPProtocol::enqueueDatagramTo(const std::vector<uint8_t>& bytes, const SocketAddress& address) {
    m_outQueue.push_back({ bytes, address });
    return true;
}

void SimpleUDPProtocol::update() {
    Protocol::update();
    if (!m_socket) return;
    uint32_t budget = 0;
    if (m_config.enablePacing) {
        if (!m_pacer) return; // pacing enabled but no pacer; nothing to send
        budget = m_pacer->dequeueBudgetBytes();
    } else {
        // if pacing disabled, allow draining all queued now
        budget = UINT32_MAX;
    }
    while (budget > 0 && !m_outQueue.empty()) {
        auto out = std::move(m_outQueue.front());
        if (out.bytes.size() > budget) break;
        m_outQueue.pop_front();
        int sent = m_socket->sendTo(out.bytes.data(), out.bytes.size(), out.addr);
        if (sent > 0) {
            m_stats.packetsSent++;
            m_stats.bytesSent += sent;
            budget -= static_cast<uint32_t>(sent);
        } else {
            break;
        }
    }
}

// New method implementations for enhanced retransmission and MTU discovery

uint32_t Protocol::getCurrentMTU() const {
    return m_currentMTU;
}

void Protocol::setMTU(uint32_t mtu) {
    m_currentMTU = mtu;
    m_config.enablePMTUDiscovery = false; // Disable PMTU discovery when manually set
}

void Protocol::setPMTUDiscovery(bool enable) {
    m_config.enablePMTUDiscovery = enable;
    if (enable) {
        m_lastMTUProbe = std::chrono::steady_clock::now();
    }
}

void Protocol::setCryptoConfig(const CryptoConfig& cfg) {
    m_cryptoConfig = cfg;
    if (m_config.enableEncryption) {
        m_cipher = CryptoFactory::create(m_cryptoConfig);
        if (m_cipher) m_cipher->initialize(m_cryptoConfig);
    }
}

void Protocol::addToSendBuffer(uint32_t sequence, const std::vector<uint8_t>& packetData, 
                              bool isReliable, PacketType type) {
    if (!isReliable) return;
    
    // Check send buffer size limit
    if (m_sendBuffer.size() >= m_config.maxSendBufferSize / 1024) { // Rough limit
        // Remove oldest packet
        auto oldest = m_sendBuffer.begin();
        for (auto it = m_sendBuffer.begin(); it != m_sendBuffer.end(); ++it) {
            if (it->second.sentTime < oldest->second.sentTime) {
                oldest = it;
            }
        }
        m_sendBuffer.erase(oldest);
    }
    
    m_sendBuffer[sequence] = SentPacket(packetData, isReliable, type);
}

void Protocol::removeFromSendBuffer(uint32_t sequence) {
    auto it = m_sendBuffer.find(sequence);
    if (it != m_sendBuffer.end()) {
        m_stats.bytesAcked += it->second.data.size();
        m_sendBuffer.erase(it);
    }
}

void Protocol::checkForLostPackets() {
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(m_config.ackTimeout);
    
    for (auto it = m_sendBuffer.begin(); it != m_sendBuffer.end();) {
        auto& sentPacket = it->second;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - sentPacket.sentTime);
        
        if (elapsed > timeout && sentPacket.retryCount < m_config.maxRetries) {
            // Retransmit packet
            int sent = m_socket->send(sentPacket.data.data(), sentPacket.data.size());
            if (sent > 0) {
                sentPacket.sentTime = now;
                sentPacket.retryCount++;
                m_stats.packetsRetransmitted++;
                m_stats.packetsSent++;
                m_stats.bytesSent += sent;
            }
            ++it;
        } else if (sentPacket.retryCount >= m_config.maxRetries) {
            // Give up on this packet
            m_stats.packetsLost++;
            it = m_sendBuffer.erase(it);
        } else {
            ++it;
        }
    }
}

void Protocol::handleFastRetransmit(uint32_t sequence) {
    if (sequence == m_lastAckedSequence) {
        m_duplicateAcks++;
        if (m_duplicateAcks >= m_config.fastRetransmitThreshold) {
            // Fast retransmit the oldest unacked packet
            auto oldest = m_sendBuffer.end();
            for (auto it = m_sendBuffer.begin(); it != m_sendBuffer.end(); ++it) {
                if (it->first < sequence && (oldest == m_sendBuffer.end() || it->first < oldest->first)) {
                    oldest = it;
                }
            }
            if (oldest != m_sendBuffer.end()) {
                int sent = m_socket->send(oldest->second.data.data(), oldest->second.data.size());
                if (sent > 0) {
                    oldest->second.sentTime = std::chrono::steady_clock::now();
                    oldest->second.retryCount++;
                    m_stats.packetsRetransmitted++;
                    m_stats.packetsSent++;
                    m_stats.bytesSent += sent;
                }
            }
            m_duplicateAcks = 0;
        }
    } else {
        m_duplicateAcks = 0;
        m_lastAckedSequence = sequence;
    }
}

bool Protocol::performMTUProbe() {
    if (m_mtuProbePending) return false;
    
    // Create a probe packet with current MTU + 100 bytes
    uint32_t probeSize = m_currentMTU + 100;
    std::vector<uint8_t> probeData(probeSize, 0xFF);
    
    Packet probePacket(PacketType::DATA, probeData.data(), probeSize);
    probePacket.setSequence(getNextSequence());
    
    std::vector<uint8_t> serialized = probePacket.serialize();
    int sent = m_socket->send(serialized.data(), serialized.size());
    
    if (sent > 0) {
        m_mtuProbePending = true;
        m_lastMTUProbe = std::chrono::steady_clock::now();
        return true;
    }
    
    return false;
}

void Protocol::handleMTUProbeResponse(bool success) {
    m_mtuProbePending = false;
    
    if (success) {
        // Increase MTU by 100 bytes
        m_currentMTU = std::min(m_currentMTU + 100, m_config.maxPacketSize);
    } else {
        // Decrease MTU by 100 bytes
        m_currentMTU = std::max(m_currentMTU - 100, m_config.conservativeMTU);
    }
}

} // namespace core
} // namespace tienet
