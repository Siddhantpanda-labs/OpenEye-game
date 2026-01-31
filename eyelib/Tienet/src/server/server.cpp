/**
 * @file server.cpp
 * @brief Generic server implementation
 */

#include "tienet/server/server.hpp"
#include "tienet/core/socket.hpp"
#include "tienet/core/protocol.hpp"
#include <iostream>
#include <algorithm>

namespace tienet {
namespace server {

// Server implementation
Server::Server() 
    : m_running(false), m_initialized(false), m_nextClientId(1) {
}

Server::~Server() {
    shutdown();
}

bool Server::initialize(const ServerConfig& config) {
    if (m_initialized) {
        return true;
    }
    
    m_config = config;
    
    // Create socket
    m_socket = core::SocketFactory::createUDPSocket();
    if (!m_socket) {
        return false;
    }
    
    // Set socket options
    m_socket->setOption("SO_BROADCAST", "true");
    m_socket->setNonBlocking(true);
    
    // Create protocol
    m_protocol = std::make_unique<core::SimpleUDPProtocol>();
    m_protocol->setSocket(m_socket);
    
    // Set protocol callbacks
    m_protocol->setDataCallback([this](const core::Packet& packet) {
        // This will be handled by the specific server implementation
    });
    
    m_initialized = true;
    return true;
}

bool Server::start() {
    if (!m_initialized) {
        return false;
    }
    
    if (m_running) {
        return true;
    }
    
    // Bind socket
    core::SocketAddress bindAddr(m_config.bindAddress, m_config.port);
    if (!m_socket->bind(bindAddr)) {
        return false;
    }
    
    m_running = true;
    m_startTime = std::chrono::steady_clock::now();
    
    notifyEvent(ServerEvent::ServerStarted, ClientInfo());
    
    return true;
}

void Server::stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    // Disconnect all clients
    disconnectAllClients();
    
    // Close socket
    if (m_socket) {
        m_socket->close();
    }
    
    notifyEvent(ServerEvent::ServerStopped, ClientInfo());
}

void Server::shutdown() {
    stop();
    
    m_socket.reset();
    m_protocol.reset();
    m_clients.clear();
    m_initialized = false;
}

void Server::update() {
    if (!m_running) {
        return;
    }
    
    // Update protocol
    if (m_protocol) {
        m_protocol->update();
    }
    
    // Cleanup inactive clients
    cleanupInactiveClients();
    
    // Send heartbeat
    sendHeartbeat();
    
    // Update statistics
    updateStats();
}

bool Server::sendToClient(uint32_t clientId, const core::Packet& packet) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientId);
    if (it == m_clients.end()) {
        return false;
    }
    
    const ClientInfo& client = it->second;
    
    // Send packet to client
    if (m_protocol) {
        return m_protocol->sendPacketTo(packet, client.address);
    }
    
    return false;
}

bool Server::sendToClient(uint32_t clientId, const void* data, size_t size, core::PacketType type) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    auto it = m_clients.find(clientId);
    if (it == m_clients.end()) return false;
    const ClientInfo& client = it->second;
    if (m_protocol) {
        return m_protocol->sendLargeTo(data, size, type, client.address);
    }
    return false;
}

uint32_t Server::broadcast(const core::Packet& packet) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    uint32_t sent = 0;
    for (const auto& pair : m_clients) {
        const ClientInfo& client = pair.second;
        if (client.isConnected) {
            if (m_protocol) {
                if (m_protocol->sendPacketTo(packet, client.address)) {
                    sent++;
                }
            }
        }
    }
    
    return sent;
}

uint32_t Server::broadcast(const void* data, size_t size, core::PacketType type) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    uint32_t sent = 0;
    for (const auto& pair : m_clients) {
        const ClientInfo& client = pair.second;
        if (!client.isConnected) continue;
        if (m_protocol) {
            if (m_protocol->sendLargeTo(data, size, type, client.address)) {
                sent++;
            }
        }
    }
    return sent;
}

void Server::disconnectClient(uint32_t clientId) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientId);
    if (it != m_clients.end()) {
        ClientInfo& client = it->second;
        client.isConnected = false;
        
        notifyEvent(ServerEvent::ClientDisconnected, client);
        
        m_clients.erase(it);
    }
}

void Server::disconnectAllClients() {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    for (auto& pair : m_clients) {
        ClientInfo& client = pair.second;
        client.isConnected = false;
        notifyEvent(ServerEvent::ClientDisconnected, client);
    }
    
    m_clients.clear();
}

const ClientInfo* Server::getClient(uint32_t clientId) const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientId);
    if (it != m_clients.end()) {
        return &it->second;
    }
    
    return nullptr;
}

std::vector<ClientInfo> Server::getClients() const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    std::vector<ClientInfo> result;
    for (const auto& pair : m_clients) {
        if (pair.second.isConnected) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

uint32_t Server::getClientCount() const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    uint32_t count = 0;
    for (const auto& pair : m_clients) {
        if (pair.second.isConnected) {
            count++;
        }
    }
    
    return count;
}

bool Server::isClientConnected(uint32_t clientId) const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientId);
    return it != m_clients.end() && it->second.isConnected;
}

ServerStats Server::getStats() const {
    return m_stats;
}

void Server::resetStats() {
    m_stats = ServerStats();
}

void Server::setEventCallback(EventCallback callback) {
    m_eventCallback = callback;
}

void Server::setDataCallback(DataCallback callback) {
    m_dataCallback = callback;
}

void Server::setConfig(const ServerConfig& config) {
    m_config = config;
}

ServerConfig Server::getConfig() const {
    return m_config;
}

bool Server::isRunning() const {
    return m_running;
}

uint32_t Server::handleClientConnection(const core::SocketAddress& address) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    // Check if client already exists
    for (const auto& pair : m_clients) {
        const ClientInfo& client = pair.second;
        if (client.address.host == address.host && client.address.port == address.port) {
            return client.id;
        }
    }
    
    // Check max clients limit
    if (m_clients.size() >= m_config.maxClients) {
        return 0;
    }
    
    // Create new client
    uint32_t clientId = generateClientId();
    ClientInfo client(clientId, address);
    
    m_clients[clientId] = client;
    
    notifyEvent(ServerEvent::ClientConnected, client);
    
    return clientId;
}

void Server::handleClientDisconnection(uint32_t clientId) {
    disconnectClient(clientId);
}

void Server::handleClientData(uint32_t clientId, const core::Packet& packet) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientId);
    if (it != m_clients.end()) {
        ClientInfo& client = it->second;
        client.lastSeen = std::chrono::steady_clock::now();
        
        if (m_dataCallback) {
            m_dataCallback(client, packet);
        }
    }
}

void Server::processIncomingData(const void* data, size_t size, const core::SocketAddress& address) {
    // Try to process as packet
    core::Packet packet;
    if (packet.deserialize(data, size)) {
        // Handle as packet
        uint32_t clientId = handleClientConnection(address);
        if (clientId != 0) {
            handleClientData(clientId, packet);
        }
    } else {
        // Handle as raw data
        uint32_t clientId = handleClientConnection(address);
        if (clientId != 0) {
            core::Packet dataPacket(core::PacketType::DATA, data, size);
            handleClientData(clientId, dataPacket);
        }
    }
}

void Server::cleanupInactiveClients() {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(m_config.clientTimeout);
    
    for (auto it = m_clients.begin(); it != m_clients.end();) {
        ClientInfo& client = it->second;
        if (now - client.lastSeen > timeout) {
            client.isConnected = false;
            notifyEvent(ServerEvent::ClientDisconnected, client);
            it = m_clients.erase(it);
        } else {
            ++it;
        }
    }
}

void Server::sendHeartbeat() {
    auto now = std::chrono::steady_clock::now();
    auto interval = std::chrono::milliseconds(m_config.heartbeatInterval);
    
    if (now - m_lastHeartbeat > interval) {
        core::Packet heartbeat = core::PacketBuilder()
            .type(core::PacketType::PING)
            .build();
        
        broadcast(heartbeat);
        m_lastHeartbeat = now;
    }
}

uint32_t Server::generateClientId() {
    return m_nextClientId++;
}

void Server::updateStats() {
    m_stats.activeClients = getClientCount();
    m_stats.totalClients = m_clients.size();
    
    auto now = std::chrono::steady_clock::now();
    m_stats.uptime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count());
}

void Server::notifyEvent(ServerEvent event, const ClientInfo& client, const core::Packet& packet) {
    if (m_eventCallback) {
        m_eventCallback(event, client, packet);
    }
}

// UDPServer implementation
UDPServer::UDPServer() : Server(), m_threadRunning(false) {
}

// Destructor defined as default in header

bool UDPServer::initialize(const ServerConfig& config) {
    if (!Server::initialize(config)) {
        return false;
    }
    
    return true;
}

bool UDPServer::start() {
    if (!Server::start()) {
        return false;
    }
    
    // Start server thread
    m_threadRunning = true;
    m_serverThread = std::thread(&UDPServer::serverThreadFunction, this);
    
    return true;
}

void UDPServer::stop() {
    if (!m_running) {
        return;
    }
    
    // Stop server thread
    m_threadRunning = false;
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    
    Server::stop();
}

void UDPServer::update() {
    // UDP server doesn't need update in main thread
    // Everything is handled in the server thread
}

void UDPServer::processIncomingData(const void* data, size_t size, const core::SocketAddress& address) {
    Server::processIncomingData(data, size, address);
}

void UDPServer::serverThreadFunction() {
    std::vector<uint8_t> buffer(65536);
    
    while (m_threadRunning && m_running) {
        core::SocketAddress senderAddr;
        int received = m_socket->receiveFrom(buffer.data(), buffer.size(), senderAddr);
        
        if (received > 0) {
            handleUDPPacket(buffer.data(), received, senderAddr);
        } else if (received == -1) {
            // Error occurred
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void UDPServer::handleUDPPacket(const void* data, size_t size, const core::SocketAddress& address) {
    processIncomingData(data, size, address);
}

} // namespace server
} // namespace tienet
