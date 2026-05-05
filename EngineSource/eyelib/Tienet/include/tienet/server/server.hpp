/**
 * @file server.hpp
 * @brief Generic server implementation for network communication
 */

#pragma once

#include "tienet/core/socket.hpp"
#include "tienet/core/packet.hpp"
#include "tienet/core/protocol.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace tienet {
namespace server {

/**
 * @brief Client information
 */
struct ClientInfo {
    uint32_t id;
    core::SocketAddress address;
    std::chrono::steady_clock::time_point lastSeen;
    core::ProtocolStats stats;
    bool isConnected;
    
    ClientInfo() : id(0), isConnected(false) {}
    ClientInfo(uint32_t clientId, const core::SocketAddress& addr) 
        : id(clientId), address(addr), isConnected(true) {
        lastSeen = std::chrono::steady_clock::now();
    }
};

/**
 * @brief Server events
 */
enum class ServerEvent {
    ClientConnected,
    ClientDisconnected,
    ClientDataReceived,
    ServerStarted,
    ServerStopped,
    Error
};

/**
 * @brief Server configuration
 */
struct ServerConfig {
    uint16_t port;
    uint32_t maxClients;
    uint32_t clientTimeout;
    uint32_t heartbeatInterval;
    bool enableBroadcast;
    bool enableMulticast;
    std::string bindAddress;
    
    ServerConfig() : port(8080), maxClients(100), clientTimeout(30000),
                    heartbeatInterval(1000), enableBroadcast(false),
                    enableMulticast(false), bindAddress("0.0.0.0") {}
};

/**
 * @brief Server statistics
 */
struct ServerStats {
    uint32_t totalClients;
    uint32_t activeClients;
    uint64_t totalPacketsSent;
    uint64_t totalPacketsReceived;
    uint64_t totalBytesSent;
    uint64_t totalBytesReceived;
    uint32_t uptime;
    
    ServerStats() : totalClients(0), activeClients(0), totalPacketsSent(0),
                   totalPacketsReceived(0), totalBytesSent(0), totalBytesReceived(0),
                   uptime(0) {}
};

/**
 * @brief Generic server class
 */
class Server {
public:
    using EventCallback = std::function<void(ServerEvent event, const ClientInfo& client, const core::Packet& packet)>;
    using DataCallback = std::function<void(const ClientInfo& client, const core::Packet& packet)>;
    
    Server();
    virtual ~Server();
    
    /**
     * @brief Initialize server
     * @param config Server configuration
     * @return true if successful, false otherwise
     */
    virtual bool initialize(const ServerConfig& config = ServerConfig());
    
    /**
     * @brief Start server
     * @return true if successful, false otherwise
     */
    virtual bool start();
    
    /**
     * @brief Stop server
     */
    virtual void stop();
    
    /**
     * @brief Shutdown server
     */
    virtual void shutdown();
    
    /**
     * @brief Update server (call in main loop)
     */
    virtual void update();
    
    /**
     * @brief Send packet to specific client
     * @param clientId Client ID
     * @param packet Packet to send
     * @return true if successful, false otherwise
     */
    virtual bool sendToClient(uint32_t clientId, const core::Packet& packet);
    
    /**
     * @brief Send data to specific client
     * @param clientId Client ID
     * @param data Data to send
     * @param size Data size
     * @param type Packet type
     * @return true if successful, false otherwise
     */
    virtual bool sendToClient(uint32_t clientId, const void* data, size_t size, core::PacketType type = core::PacketType::DATA);
    
    /**
     * @brief Broadcast packet to all clients
     * @param packet Packet to broadcast
     * @return Number of clients packet was sent to
     */
    virtual uint32_t broadcast(const core::Packet& packet);
    
    /**
     * @brief Broadcast data to all clients
     * @param data Data to broadcast
     * @param size Data size
     * @param type Packet type
     * @return Number of clients data was sent to
     */
    virtual uint32_t broadcast(const void* data, size_t size, core::PacketType type = core::PacketType::DATA);
    
    /**
     * @brief Disconnect specific client
     * @param clientId Client ID
     */
    virtual void disconnectClient(uint32_t clientId);
    
    /**
     * @brief Disconnect all clients
     */
    virtual void disconnectAllClients();
    
    /**
     * @brief Get client information
     * @param clientId Client ID
     * @return Client info, nullptr if not found
     */
    virtual const ClientInfo* getClient(uint32_t clientId) const;
    
    /**
     * @brief Get all connected clients
     * @return Vector of client information
     */
    virtual std::vector<ClientInfo> getClients() const;
    
    /**
     * @brief Get number of connected clients
     * @return Number of connected clients
     */
    virtual uint32_t getClientCount() const;
    
    /**
     * @brief Check if client is connected
     * @param clientId Client ID
     * @return true if client is connected
     */
    virtual bool isClientConnected(uint32_t clientId) const;
    
    /**
     * @brief Get server statistics
     * @return Server statistics
     */
    virtual ServerStats getStats() const;
    
    /**
     * @brief Reset server statistics
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
     * @param config Server configuration
     */
    virtual void setConfig(const ServerConfig& config);
    
    /**
     * @brief Get configuration
     * @return Server configuration
     */
    virtual ServerConfig getConfig() const;
    
    /**
     * @brief Check if server is running
     * @return true if server is running
     */
    virtual bool isRunning() const;

protected:
    /**
     * @brief Handle client connection
     * @param address Client address
     * @return Client ID, 0 if failed
     */
    virtual uint32_t handleClientConnection(const core::SocketAddress& address);
    
    /**
     * @brief Handle client disconnection
     * @param clientId Client ID
     */
    virtual void handleClientDisconnection(uint32_t clientId);
    
    /**
     * @brief Handle client data
     * @param clientId Client ID
     * @param packet Received packet
     */
    virtual void handleClientData(uint32_t clientId, const core::Packet& packet);
    
    /**
     * @brief Process incoming data
     * @param data Received data
     * @param size Data size
     * @param address Source address
     */
    virtual void processIncomingData(const void* data, size_t size, const core::SocketAddress& address);
    
    /**
     * @brief Cleanup inactive clients
     */
    virtual void cleanupInactiveClients();
    
    /**
     * @brief Send heartbeat to clients
     */
    virtual void sendHeartbeat();
    
    /**
     * @brief Generate unique client ID
     * @return Unique client ID
     */
    virtual uint32_t generateClientId();

private:
    ServerConfig m_config;
    ServerStats m_stats;
    
    std::unordered_map<uint32_t, ClientInfo> m_clients;
    mutable std::mutex m_clientsMutex;
    
protected:
    std::shared_ptr<core::Socket> m_socket;
    std::unique_ptr<core::Protocol> m_protocol;
    
    std::atomic<bool> m_running;
    std::atomic<bool> m_initialized;
    
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_lastHeartbeat;
    
    EventCallback m_eventCallback;
    DataCallback m_dataCallback;
    
    uint32_t m_nextClientId;
    
    void updateStats();
    void notifyEvent(ServerEvent event, const ClientInfo& client, const core::Packet& packet = core::Packet());
};

/**
 * @brief UDP server implementation
 */
class UDPServer : public Server {
public:
    UDPServer();
    virtual ~UDPServer() = default;
    
    /**
     * @brief Initialize UDP server
     * @param config Server configuration
     * @return true if successful, false otherwise
     */
    virtual bool initialize(const ServerConfig& config = ServerConfig()) override;
    
    /**
     * @brief Start UDP server
     * @return true if successful, false otherwise
     */
    virtual bool start() override;
    
    /**
     * @brief Stop UDP server
     */
    virtual void stop() override;
    
    /**
     * @brief Update UDP server
     */
    virtual void update() override;

protected:
    virtual void processIncomingData(const void* data, size_t size, const core::SocketAddress& address) override;

private:
    std::thread m_serverThread;
    std::atomic<bool> m_threadRunning;
    
    void serverThreadFunction();
    void handleUDPPacket(const void* data, size_t size, const core::SocketAddress& address);
};

} // namespace server
} // namespace tienet
