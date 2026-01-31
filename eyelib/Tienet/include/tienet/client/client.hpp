/**
 * @file client.hpp
 * @brief Generic client implementation for network communication
 */

#pragma once

#include "tienet/core/socket.hpp"
#include "tienet/core/packet.hpp"
#include "tienet/core/protocol.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

namespace tienet {
namespace client {

/**
 * @brief Client events
 */
enum class ClientEvent {
    Connected,
    Disconnected,
    DataReceived,
    Error
};

/**
 * @brief Client configuration
 */
struct ClientConfig {
    std::string serverAddress;
    uint16_t serverPort;
    uint32_t connectionTimeout;
    uint32_t heartbeatInterval;
    uint32_t reconnectInterval;
    uint32_t maxReconnectAttempts;
    bool autoReconnect;
    bool enableHeartbeat;
    
    ClientConfig() : serverAddress("127.0.0.1"), serverPort(8080), connectionTimeout(10000),
                    heartbeatInterval(1000), reconnectInterval(5000), maxReconnectAttempts(5),
                    autoReconnect(true), enableHeartbeat(true) {}
};

/**
 * @brief Client statistics
 */
struct ClientStats {
    uint64_t packetsSent;
    uint64_t packetsReceived;
    uint64_t bytesSent;
    uint64_t bytesReceived;
    uint32_t connectionTime;
    uint32_t lastLatency;
    uint32_t averageLatency;
    uint32_t reconnectAttempts;
    
    ClientStats() : packetsSent(0), packetsReceived(0), bytesSent(0), bytesReceived(0),
                   connectionTime(0), lastLatency(0), averageLatency(0), reconnectAttempts(0) {}
};

/**
 * @brief Generic client class
 */
class Client {
public:
    using EventCallback = std::function<void(ClientEvent event, const core::Packet& packet)>;
    using DataCallback = std::function<void(const core::Packet& packet)>;
    
    Client();
    virtual ~Client();
    
    /**
     * @brief Initialize client
     * @param config Client configuration
     * @return true if successful, false otherwise
     */
    virtual bool initialize(const ClientConfig& config = ClientConfig());
    
    /**
     * @brief Connect to server
     * @param address Server address
     * @param port Server port
     * @return true if successful, false otherwise
     */
    virtual bool connect(const std::string& address, uint16_t port);
    
    /**
     * @brief Connect to server
     * @param address Server address
     * @return true if successful, false otherwise
     */
    virtual bool connect(const core::SocketAddress& address);
    
    /**
     * @brief Disconnect from server
     */
    virtual void disconnect();
    
    /**
     * @brief Shutdown client
     */
    virtual void shutdown();
    
    /**
     * @brief Update client (call in main loop)
     */
    virtual void update();
    // Expose current target bitrate from transport (for ABR binding)
    uint32_t getTransportTargetBitrateBps() const { return m_protocol ? m_protocol->getTargetBitrateBps() : 0; }
    
    // Runtime protocol configuration
    void setProtocolBitrate(uint32_t bps);
    void enableProtocolPacing(bool enabled);
    void enableProtocolFEC(bool enabled, uint32_t groupSize = 8);
    void setProtocolFECParity(uint32_t parityCount);
    void setProtocolFECInterleave(uint32_t stride);
    void setProtocolMTU(uint32_t mtuBytes);

    // NAT traversal (Phase 5): STUN Binding to discover public endpoint
    // Returns true if success and fills outPublic with server-reflected address
    bool stunResolvePublicEndpoint(const core::SocketAddress& stunServer, core::SocketAddress& outPublic, uint32_t timeoutMs = 1000);
    
    /**
     * @brief Send packet to server
     * @param packet Packet to send
     * @return true if successful, false otherwise
     */
    virtual bool sendPacket(const core::Packet& packet);
    
    /**
     * @brief Send data to server
     * @param data Data to send
     * @param size Data size
     * @param type Packet type
     * @return true if successful, false otherwise
     */
    virtual bool sendData(const void* data, size_t size, core::PacketType type = core::PacketType::DATA);
    
    /**
     * @brief Send string to server
     * @param message String message
     * @return true if successful, false otherwise
     */
    virtual bool sendMessage(const std::string& message);
    
    /**
     * @brief Check if client is connected
     * @return true if connected
     */
    virtual bool isConnected() const;
    
    /**
     * @brief Get client statistics
     * @return Client statistics
     */
    virtual ClientStats getStats() const;
    
    /**
     * @brief Reset client statistics
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
     * @param config Client configuration
     */
    virtual void setConfig(const ClientConfig& config);
    
    /**
     * @brief Get configuration
     * @return Client configuration
     */
    virtual ClientConfig getConfig() const;
    
    /**
     * @brief Get server address
     * @return Server address
     */
    virtual core::SocketAddress getServerAddress() const;

protected:
    /**
     * @brief Handle connection event
     * @param connected Connection state
     */
    virtual void handleConnection(bool connected);
    
    /**
     * @brief Handle data received
     * @param packet Received packet
     */
    virtual void handleDataReceived(const core::Packet& packet);
    
    /**
     * @brief Handle error event
     * @param error Error message
     */
    virtual void handleError(const std::string& error);
    
    /**
     * @brief Process incoming data
     * @param data Received data
     * @param size Data size
     */
    virtual void processIncomingData(const void* data, size_t size);
    
    /**
     * @brief Send heartbeat to server
     */
    virtual void sendHeartbeat();
    
    /**
     * @brief Attempt to reconnect
     * @return true if reconnection successful
     */
    virtual bool attemptReconnect();

private:
    ClientConfig m_config;
    ClientStats m_stats;
    
protected:
    std::shared_ptr<core::Socket> m_socket;
    std::unique_ptr<core::Protocol> m_protocol;
    
    core::SocketAddress m_serverAddress;
    std::atomic<bool> m_connected;
    std::atomic<bool> m_initialized;
    
    std::chrono::steady_clock::time_point m_connectionTime;
    std::chrono::steady_clock::time_point m_lastHeartbeat;
    std::chrono::steady_clock::time_point m_lastReconnectAttempt;
    
    EventCallback m_eventCallback;
    DataCallback m_dataCallback;
    
    void updateStats();
    void notifyEvent(ClientEvent event, const core::Packet& packet = core::Packet());
};

/**
 * @brief UDP client implementation
 */
class UDPClient : public Client {
public:
    UDPClient();
    virtual ~UDPClient() = default;
    
    /**
     * @brief Initialize UDP client
     * @param config Client configuration
     * @return true if successful, false otherwise
     */
    virtual bool initialize(const ClientConfig& config = ClientConfig()) override;
    
    /**
     * @brief Connect to UDP server
     * @param address Server address
     * @param port Server port
     * @return true if successful, false otherwise
     */
    virtual bool connect(const std::string& address, uint16_t port) override;
    
    /**
     * @brief Connect to UDP server
     * @param address Server address
     * @return true if successful, false otherwise
     */
    virtual bool connect(const core::SocketAddress& address) override;
    
    /**
     * @brief Disconnect from server
     */
    virtual void disconnect() override;
    
    /**
     * @brief Update UDP client
     */
    virtual void update() override;

protected:
    virtual void processIncomingData(const void* data, size_t size) override;

private:
    std::thread m_clientThread;
    std::atomic<bool> m_threadRunning;
    
    void clientThreadFunction();
    void handleUDPPacket(const void* data, size_t size);
};

} // namespace client
} // namespace tienet
