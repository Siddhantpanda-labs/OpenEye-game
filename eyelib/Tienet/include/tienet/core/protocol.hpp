/**
 * @file protocol.hpp
 * @brief Generic protocol handler for network communication
 */

#pragma once

#include "tienet/core/packet.hpp"
#include "tienet/core/socket.hpp"
#include "tienet/core/security.hpp"
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <deque> // Added for pacing/FEC state
#include <chrono>

namespace tienet {
namespace core {

class RateController;
class Pacer;

/**
 * @brief Protocol states
 */
enum class ProtocolState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

/**
 * @brief Protocol events
 */
enum class ProtocolEvent {
    Connected,
    Disconnected,
    DataReceived,
    Error
};

/**
 * @brief Protocol statistics
 */
struct ProtocolStats {
    uint64_t packetsSent;
    uint64_t packetsReceived;
    uint64_t bytesSent;
    uint64_t bytesReceived;
    uint64_t packetsLost;
    uint64_t packetsDropped;
    uint64_t packetsRetransmitted;
    uint64_t bytesAcked;
    uint64_t bytesGoodput;
    uint32_t averageLatency;
    uint32_t maxLatency;
    uint32_t minLatency;
    uint32_t queueingDelayMs;
    uint32_t probeOutcomes;
    
    ProtocolStats() : packetsSent(0), packetsReceived(0), bytesSent(0), 
                     bytesReceived(0), packetsLost(0), packetsDropped(0),
                     packetsRetransmitted(0), bytesAcked(0), bytesGoodput(0),
                     averageLatency(0), maxLatency(0), minLatency(UINT32_MAX),
                     queueingDelayMs(0), probeOutcomes(0) {}
};

/**
 * @brief Protocol configuration
 */
struct ProtocolConfig {
    uint32_t maxPacketSize;
    uint32_t maxFragments;
    uint32_t fragmentTimeout;
    uint32_t ackTimeout;
    uint32_t maxRetries;
    uint32_t heartbeatInterval;
    uint32_t connectionTimeout;
    bool enableCompression;
    bool enableEncryption;
    bool enableReliability;
    
    // New: pacing / rate control / FEC
    bool enablePacing;
    bool enableFEC;
    bool enableAdaptiveBitrate;
    uint32_t initialTargetBitrateBps;
    uint32_t minBitrateBps;
    uint32_t maxBitrateBps;
    uint32_t fecGroupSize; // N data packets per parity
    uint32_t fecParityCount; // number of parity packets per group
    uint32_t fecInterleaveStride; // interleaving stride among groups
    
    // Enhanced: MTU discovery and retransmission
    bool enablePMTUDiscovery;
    uint32_t conservativeMTU;
    uint32_t maxSendBufferSize;
    uint32_t fastRetransmitThreshold;
    uint32_t reorderingWindowSize;
    
    ProtocolConfig() : maxPacketSize(65536), maxFragments(16), fragmentTimeout(5000),
                      ackTimeout(1000), maxRetries(3), heartbeatInterval(1000),
                      connectionTimeout(10000), enableCompression(false),
                      enableEncryption(false), enableReliability(true),
                      enablePacing(true), enableFEC(false), enableAdaptiveBitrate(true),
                      initialTargetBitrateBps(4 * 1024 * 1024),
                      minBitrateBps(128 * 1024), maxBitrateBps(50 * 1024 * 1024),
                      fecGroupSize(8), fecParityCount(1), fecInterleaveStride(1),
                      enablePMTUDiscovery(false), conservativeMTU(1200),
                      maxSendBufferSize(1024 * 1024), fastRetransmitThreshold(3),
                      reorderingWindowSize(32) {}
};

/**
 * @brief Abstract protocol handler
 */
class Protocol {
public:
    using EventCallback = std::function<void(ProtocolEvent event, const Packet& packet)>;
    using DataCallback = std::function<void(const Packet& packet)>;
    
    Protocol();
    virtual ~Protocol() = default;
    
    /**
     * @brief Initialize protocol
     * @param config Protocol configuration
     * @return true if successful, false otherwise
     */
    virtual bool initialize(const ProtocolConfig& config = ProtocolConfig());
    
    /**
     * @brief Shutdown protocol
     */
    virtual void shutdown();
    
    /**
     * @brief Set socket for communication
     * @param socket Socket to use
     */
    virtual void setSocket(std::shared_ptr<Socket> socket);
    
    /**
     * @brief Connect to remote address
     * @param address Remote address
     * @return true if successful, false otherwise
     */
    virtual bool connect(const SocketAddress& address);
    
    /**
     * @brief Disconnect from remote
     */
    virtual void disconnect();
    
    /**
     * @brief Send packet
     * @param packet Packet to send
     * @return true if successful, false otherwise
     */
    virtual bool sendPacket(const Packet& packet);
    // MTU-aware send that fragments if needed
    virtual bool sendLarge(const void* data, size_t size, PacketType type = PacketType::DATA);
    // MTU-aware send to a specific address (for server side)
    virtual bool sendLargeTo(const void* data, size_t size, PacketType type, const SocketAddress& address) { (void)data; (void)size; (void)type; (void)address; return false; }
    
    // New: enqueue raw datagram after serialization (used by pacer/FEC integration)
    virtual bool enqueueDatagram(const std::vector<uint8_t>& bytes);
    // New: enqueue datagram to a specific address (for UDP servers)
    virtual bool enqueueDatagramTo(const std::vector<uint8_t>& bytes, const SocketAddress& address) { (void)bytes; (void)address; return false; }
    
    /**
     * @brief Send data
     * @param data Data to send
     * @param size Data size
     * @param type Packet type
     * @return true if successful, false otherwise
     */
    virtual bool sendData(const void* data, size_t size, PacketType type = PacketType::DATA);
    
    /**
     * @brief Process incoming data
     * @param data Received data
     * @param size Data size
     * @return true if processed successfully, false otherwise
     */
    virtual bool processData(const void* data, size_t size);

    // Optional: send to a specific remote address (useful for UDP)
    // Default implementation returns false; override in derived protocols
    virtual bool sendPacketTo(const Packet& packet, const SocketAddress& address) { (void)packet; (void)address; return false; }
    
    /**
     * @brief Update protocol (call in main loop)
     */
    virtual void update();
    
    /**
     * @brief Get protocol state
     * @return Current protocol state
     */
    virtual ProtocolState getState() const;
    
    /**
     * @brief Get protocol statistics
     * @return Protocol statistics
     */
    virtual ProtocolStats getStats() const;
    
    /**
     * @brief Reset statistics
     */
    virtual void resetStats();
    
    /**
     * @brief Set event callback
     * @param callback Event callback function
     */
    virtual void setEventCallback(EventCallback callback);
    
    /**
     * @brief Set data callback
     * @param callback Data callback function
     */
    virtual void setDataCallback(DataCallback callback);
    
    /**
     * @brief Set configuration
     * @param config Protocol configuration
     */
    virtual void setConfig(const ProtocolConfig& config);
    
    /**
     * @brief Get configuration
     * @return Protocol configuration
     */
    virtual ProtocolConfig getConfig() const;
    
    /**
     * @brief Get current MTU
     * @return Current MTU in bytes
     */
    virtual uint32_t getCurrentMTU() const;
    
    /**
     * @brief Set MTU manually (disables PMTU discovery)
     * @param mtu MTU value in bytes
     */
    virtual void setMTU(uint32_t mtu);
    
    /**
     * @brief Enable/disable PMTU discovery
     * @param enable Whether to enable PMTU discovery
     */
    virtual void setPMTUDiscovery(bool enable);

    /**
     * @brief Configure crypto for this protocol session
     * @param cfg Crypto configuration (enable + algorithm + keys)
     */
    virtual void setCryptoConfig(const CryptoConfig& cfg);

    /**
     * @brief Get current target send bitrate as decided by rate control
     * @return Bitrate in bps
     */
    virtual uint32_t getTargetBitrateBps() const;

protected:
    /**
     * @brief Handle incoming packet
     * @param packet Received packet
     */
    virtual void handlePacket(const Packet& packet);
    
    /**
     * @brief Handle connection event
     * @param connected Connection state
     */
    virtual void handleConnection(bool connected);
    
    /**
     * @brief Handle error event
     * @param error Error message
     */
    virtual void handleError(const std::string& error);
    
    /**
     * @brief Send acknowledgment
     * @param sequence Sequence number to acknowledge
     */
    virtual void sendAck(uint32_t sequence);
    
    /**
     * @brief Send negative acknowledgment
     * @param sequence Sequence number to NACK
     */
    virtual void sendNack(uint32_t sequence);
    
    /**
     * @brief Process fragmented packet
     * @param packet Fragment packet
     */
    virtual void processFragment(const Packet& packet);
    
    /**
     * @brief Reassemble fragmented packet
     * @param sequence Sequence number
     * @return Reassembled packet, nullptr if not complete
     */
    virtual std::unique_ptr<Packet> reassemblePacket(uint32_t sequence);

    // New: helpers for pacing and FEC
    virtual void applyRateUpdate();
    virtual void drainSendQueue();
    // Expose sequence generator for derived protocols (e.g., fragmentation grouping)
    uint32_t getNextSequence();
    // Update receive-side reordering window (SACK tracking)
    void updateRecvWindow(uint32_t sequence);
    
    /**
     * @brief Add packet to send buffer for retransmission
     * @param sequence Packet sequence number
     * @param packetData Serialized packet data
     * @param isReliable Whether packet requires reliability
     * @param type Packet type
     */
    void addToSendBuffer(uint32_t sequence, const std::vector<uint8_t>& packetData, 
                        bool isReliable, PacketType type);
    
    /**
     * @brief Remove packet from send buffer (acknowledged)
     * @param sequence Packet sequence number
     */
    void removeFromSendBuffer(uint32_t sequence);
    
    /**
     * @brief Check for lost packets and trigger retransmission
     */
    void checkForLostPackets();
    
    /**
     * @brief Handle fast retransmit on duplicate ACKs
     * @param sequence Sequence number of duplicate ACK
     */
    void handleFastRetransmit(uint32_t sequence);
    
    /**
     * @brief Perform MTU probe
     * @return true if probe was sent, false otherwise
     */
    bool performMTUProbe();
    
    /**
     * @brief Handle MTU probe response
     * @param success Whether probe was successful
     */
    void handleMTUProbeResponse(bool success);

protected:
    // New: pacing and rate control
    std::unique_ptr<Pacer> m_pacer;
    std::unique_ptr<RateController> m_rateController;
    std::deque<std::vector<uint8_t>> m_sendQueue;
    std::unique_ptr<ICipher> m_cipher;
    CryptoConfig m_cryptoConfig;
    uint32_t m_targetBitrateBps = 0;
    
    // FEC receive groups
    struct FECRecvGroup {
        uint8_t groupSize = 0;
        std::vector<bool> present;
        std::vector<std::vector<uint8_t>> data; // serialized packets
        bool hasParity = false;
        std::vector<uint8_t> parity;
        // Multi-parity support
        std::vector<std::vector<uint8_t>> parities; // support k>=1 parities
    };
    std::unordered_map<uint16_t, FECRecvGroup> m_fecRecvGroups;

protected:
    ProtocolState m_state;
    ProtocolConfig m_config;
    ProtocolStats m_stats;
    
    std::shared_ptr<Socket> m_socket;
    EventCallback m_eventCallback;
    DataCallback m_dataCallback;
    
    // Persistent send buffer for reliable retransmission
    struct SentPacket {
        std::vector<uint8_t> data;
        std::chrono::steady_clock::time_point sentTime;
        uint32_t retryCount;
        bool isReliable;
        PacketType type;
        
        SentPacket() : sentTime(std::chrono::steady_clock::now()), retryCount(0), isReliable(false), type(PacketType::DATA) {}
        
        SentPacket(const std::vector<uint8_t>& packetData, bool reliable, PacketType pktType)
            : data(packetData), sentTime(std::chrono::steady_clock::now()), 
              retryCount(0), isReliable(reliable), type(pktType) {}
    };
    std::unordered_map<uint32_t, SentPacket> m_sendBuffer;
    
    // MTU discovery state
    uint32_t m_currentMTU;
    std::chrono::steady_clock::time_point m_lastMTUProbe;
    bool m_mtuProbePending;
    
    // Fast retransmit state
    uint32_t m_duplicateAcks;
    uint32_t m_lastAckedSequence;
    
private:
    
    uint32_t m_nextSequence;
    uint32_t m_lastAckSequence;
    // Receive-side reordering window (SACK)
    uint32_t m_recvAckBase = 0;        // Highest contiguous received sequence
    uint32_t m_recvWindowMask = 0;     // Bit i acknowledges (ackBase + i + 1)
    uint32_t m_recvHighestSeq = 0;     // Highest sequence number observed
    
    // Fragment reassembly
    std::unordered_map<uint32_t, std::vector<std::unique_ptr<Packet>>> m_fragments;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> m_fragmentTimestamps;
    
    // Reliability
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> m_sentPackets;
    std::unordered_map<uint32_t, uint32_t> m_retryCount;
    
    // New: FEC state
    std::vector<std::vector<uint8_t>> m_fecGroup;
    
    // Legacy/private helpers still used in implementation
    void cleanupExpiredFragments();
    void cleanupExpiredPackets();
    void updateLatency(uint32_t latency);
    bool isPacketReliable(const Packet& packet) const;
};

/**
 * @brief Simple UDP protocol implementation
 */
class SimpleUDPProtocol : public Protocol {
public:
    SimpleUDPProtocol();
    virtual ~SimpleUDPProtocol() = default;
    
    /**
     * @brief Send packet to specific address
     * @param packet Packet to send
     * @param address Destination address
     * @return true if successful, false otherwise
     */
    virtual bool sendPacketTo(const Packet& packet, const SocketAddress& address);
    virtual bool sendLargeTo(const void* data, size_t size, PacketType type, const SocketAddress& address) override;
    
    /**
     * @brief Process incoming data from specific address
     * @param data Received data
     * @param size Data size
     * @param address Source address
     * @return true if processed successfully, false otherwise
     */
    virtual bool processDataFrom(const void* data, size_t size, const SocketAddress& address);

protected:
    virtual void handlePacket(const Packet& packet) override;
    virtual bool sendPacket(const Packet& packet) override;
    virtual bool enqueueDatagramTo(const std::vector<uint8_t>& bytes, const SocketAddress& address) override;
    virtual void update() override;

private:
    struct OutTo { std::vector<uint8_t> bytes; SocketAddress addr; };
    std::deque<OutTo> m_outQueue;
    // FEC grouping per address (simplified)
    std::vector<OutTo> m_fecGroupTo;
    SocketAddress m_fecGroupAddr;
    bool m_fecGroupActive = false;
    uint32_t m_fecGroupFirstSeq = 0;
};

} // namespace core
} // namespace tienet
