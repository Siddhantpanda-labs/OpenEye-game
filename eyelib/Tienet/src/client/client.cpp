/**
 * @file client.cpp
 * @brief Generic client implementation
 */

#include "tienet/client/client.hpp"
#include "tienet/core/socket.hpp"
#include "tienet/core/protocol.hpp"
#include <iostream>
#include <thread>
#include <random>
#include <cstring>

namespace tienet {
namespace client {

// Client implementation
Client::Client() 
    : m_connected(false), m_initialized(false) {
}

Client::~Client() {
    shutdown();
}

bool Client::initialize(const ClientConfig& config) {
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
    m_socket->setNonBlocking(true);
    
    // Create protocol
    m_protocol = std::make_unique<core::SimpleUDPProtocol>();
    m_protocol->setSocket(m_socket);
    
    // Set protocol callbacks
    m_protocol->setEventCallback([this](core::ProtocolEvent event, const core::Packet& packet) {
        switch (event) {
            case core::ProtocolEvent::Connected:
                handleConnection(true);
                break;
            case core::ProtocolEvent::Disconnected:
                handleConnection(false);
                break;
            case core::ProtocolEvent::Error:
                handleError("Protocol error");
                break;
            default:
                break;
        }
    });
    
    m_protocol->setDataCallback([this](const core::Packet& packet) {
        handleDataReceived(packet);
    });
    
    m_initialized = true;
    return true;
}

bool Client::connect(const std::string& address, uint16_t port) {
    core::SocketAddress serverAddr(address, port);
    return connect(serverAddr);
}

bool Client::connect(const core::SocketAddress& address) {
    if (!m_initialized) {
        return false;
    }
    
    m_serverAddress = address;
    
    // Connect to server
    if (!m_protocol->connect(address)) {
        return false;
    }
    
    m_connected = true;
    m_connectionTime = std::chrono::steady_clock::now();
    
    return true;
}

void Client::disconnect() {
    if (!m_connected) {
        return;
    }
    
    m_protocol->disconnect();
    m_connected = false;
}

void Client::shutdown() {
    disconnect();
    
    m_socket.reset();
    m_protocol.reset();
    m_initialized = false;
}

void Client::update() {
    if (!m_initialized || !m_connected) {
        return;
    }
    
    // Update protocol
    if (m_protocol) {
        m_protocol->update();
    }
    
    // Send heartbeat
    sendHeartbeat();
    
    // Update statistics
    updateStats();
    
    // Check for reconnection
    if (m_config.autoReconnect && !m_connected) {
        attemptReconnect();
    }
}

void Client::setProtocolBitrate(uint32_t bps) {
    if (!m_protocol) return;
    auto cfg = m_protocol->getConfig();
    cfg.initialTargetBitrateBps = bps;
    if (bps < cfg.minBitrateBps) cfg.minBitrateBps = bps;
    if (bps > cfg.maxBitrateBps) cfg.maxBitrateBps = bps;
    m_protocol->setConfig(cfg);
}

void Client::enableProtocolPacing(bool enabled) {
    if (!m_protocol) return;
    auto cfg = m_protocol->getConfig();
    cfg.enablePacing = enabled;
    m_protocol->setConfig(cfg);
}

void Client::enableProtocolFEC(bool enabled, uint32_t groupSize) {
    if (!m_protocol) return;
    auto cfg = m_protocol->getConfig();
    cfg.enableFEC = enabled;
    cfg.fecGroupSize = groupSize > 1 ? groupSize : cfg.fecGroupSize;
    m_protocol->setConfig(cfg);
}

void Client::setProtocolFECParity(uint32_t parityCount) {
    if (!m_protocol) return;
    auto cfg = m_protocol->getConfig();
    cfg.fecParityCount = parityCount;
    m_protocol->setConfig(cfg);
}

void Client::setProtocolFECInterleave(uint32_t stride) {
    if (!m_protocol) return;
    auto cfg = m_protocol->getConfig();
    cfg.fecInterleaveStride = stride == 0 ? 1u : stride;
    m_protocol->setConfig(cfg);
}

void Client::setProtocolMTU(uint32_t mtuBytes) {
    if (!m_protocol) return;
    auto cfg = m_protocol->getConfig();
    cfg.maxPacketSize = mtuBytes;
    m_protocol->setConfig(cfg);
}

bool Client::stunResolvePublicEndpoint(const core::SocketAddress& stunServer, core::SocketAddress& outPublic, uint32_t timeoutMs) {
    // RFC5389: Binding Request with magic cookie 0x2112A442 and 12-byte transaction id
    uint8_t req[20] = {0};
    // Message Type: 0x0001 (Binding Request)
    req[0] = 0x00; req[1] = 0x01;
    // Message Length: 0 (no attributes)
    req[2] = 0x00; req[3] = 0x00;
    // Magic Cookie
    req[4] = 0x21; req[5] = 0x12; req[6] = 0xA4; req[7] = 0x42;
    // Transaction ID (12 bytes random)
    std::random_device rd; std::mt19937 gen(rd()); std::uniform_int_distribution<int> dist(0, 255);
    for (int i = 0; i < 12; ++i) req[8 + i] = static_cast<uint8_t>(dist(gen));

    // Send to STUN server
    if (!m_socket) return false;
    m_socket->sendTo(req, sizeof(req), stunServer);

    auto start = std::chrono::steady_clock::now();
    std::vector<uint8_t> buf(1024);
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < timeoutMs) {
        core::SocketAddress from;
        int n = m_socket->receiveFrom(buf.data(), buf.size(), from);
        if (n <= 0) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
        if (n < 20) continue;
        // Check magic cookie and transaction id match
        if (buf[4] != 0x21 || buf[5] != 0x12 || buf[6] != (char)0xA4 || buf[7] != 0x42) continue;
        bool txMatch = true; for (int i = 0; i < 12; ++i) { if (buf[8+i] != req[8+i]) { txMatch = false; break; } }
        if (!txMatch) continue;
        // Parse attributes to find XOR-MAPPED-ADDRESS (type 0x0020) or MAPPED-ADDRESS (0x0001)
        uint16_t msgLen = (buf[2] << 8) | buf[3];
        size_t pos = 20; size_t end = 20 + msgLen; if (end > static_cast<size_t>(n)) end = static_cast<size_t>(n);
        while (pos + 4 <= end) {
            uint16_t at = (buf[pos] << 8) | buf[pos+1];
            uint16_t al = (buf[pos+2] << 8) | buf[pos+3];
            pos += 4;
            if (pos + al > end) break;
            if (at == 0x0020 || at == 0x0001) {
                // Family
                if (al >= 8 && buf[pos+1] == 0x01) { // IPv4
                    uint16_t port = (buf[pos+2] << 8) | buf[pos+3];
                    uint32_t ip = (buf[pos+4] << 24) | (buf[pos+5] << 16) | (buf[pos+6] << 8) | buf[pos+7];
                    if (at == 0x0020) {
                        // XOR with magic cookie
                        port ^= 0x2112;
                        ip ^= 0x2112A442;
                    }
                    outPublic.port = port;
                    outPublic.host = std::to_string((ip >> 24) & 0xFF) + "." +
                                     std::to_string((ip >> 16) & 0xFF) + "." +
                                     std::to_string((ip >> 8) & 0xFF) + "." +
                                     std::to_string(ip & 0xFF);
                    return true;
                }
            }
            // 32-bit align
            pos += al; pos = (pos + 3) & ~static_cast<size_t>(3);
        }
    }
    return false;
}

bool Client::sendPacket(const core::Packet& packet) {
    if (!m_connected || !m_protocol) {
        return false;
    }
    
    return m_protocol->sendPacket(packet);
}

bool Client::sendData(const void* data, size_t size, core::PacketType type) {
    core::Packet packet(type, data, size);
    return sendPacket(packet);
}

bool Client::sendMessage(const std::string& message) {
    return sendData(message.c_str(), message.length(), core::PacketType::DATA);
}

bool Client::isConnected() const {
    return m_connected;
}

ClientStats Client::getStats() const {
    return m_stats;
}

void Client::resetStats() {
    m_stats = ClientStats();
}

void Client::setEventCallback(EventCallback callback) {
    m_eventCallback = callback;
}

void Client::setDataCallback(DataCallback callback) {
    m_dataCallback = callback;
}

void Client::setConfig(const ClientConfig& config) {
    m_config = config;
}

ClientConfig Client::getConfig() const {
    return m_config;
}

core::SocketAddress Client::getServerAddress() const {
    return m_serverAddress;
}

void Client::handleConnection(bool connected) {
    m_connected = connected;
    
    if (connected) {
        notifyEvent(ClientEvent::Connected);
    } else {
        notifyEvent(ClientEvent::Disconnected);
    }
}

void Client::handleDataReceived(const core::Packet& packet) {
    if (m_dataCallback) {
        m_dataCallback(packet);
    }
}

void Client::handleError(const std::string& error) {
    m_connected = false;
    notifyEvent(ClientEvent::Error);
    
    std::cerr << "Client error: " << error << std::endl;
}

void Client::processIncomingData(const void* data, size_t size) {
    if (m_protocol) {
        m_protocol->processData(data, size);
    }
}

void Client::sendHeartbeat() {
    if (!m_config.enableHeartbeat || !m_connected) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto interval = std::chrono::milliseconds(m_config.heartbeatInterval);
    
    if (now - m_lastHeartbeat > interval) {
        core::Packet heartbeat = core::PacketBuilder()
            .type(core::PacketType::PING)
            .build();
        
        sendPacket(heartbeat);
        m_lastHeartbeat = now;
    }
}

bool Client::attemptReconnect() {
    auto now = std::chrono::steady_clock::now();
    auto interval = std::chrono::milliseconds(m_config.reconnectInterval);
    
    if (now - m_lastReconnectAttempt > interval) {
        m_lastReconnectAttempt = now;
        m_stats.reconnectAttempts++;
        
        if (m_stats.reconnectAttempts <= m_config.maxReconnectAttempts) {
            return connect(m_serverAddress);
        }
    }
    
    return false;
}

void Client::updateStats() {
    if (m_protocol) {
        auto protocolStats = m_protocol->getStats();
        m_stats.packetsSent = protocolStats.packetsSent;
        m_stats.packetsReceived = protocolStats.packetsReceived;
        m_stats.bytesSent = protocolStats.bytesSent;
        m_stats.bytesReceived = protocolStats.bytesReceived;
        m_stats.lastLatency = protocolStats.averageLatency;
        m_stats.averageLatency = protocolStats.averageLatency;
    }
    
    if (m_connected) {
        auto now = std::chrono::steady_clock::now();
        m_stats.connectionTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_connectionTime).count());
    }
}

void Client::notifyEvent(ClientEvent event, const core::Packet& packet) {
    if (m_eventCallback) {
        m_eventCallback(event, packet);
    }
}

// UDPClient implementation
UDPClient::UDPClient() : Client(), m_threadRunning(false) {
}

// Destructor defined as default in header

bool UDPClient::initialize(const ClientConfig& config) {
    if (!Client::initialize(config)) {
        return false;
    }
    
    return true;
}

bool UDPClient::connect(const std::string& address, uint16_t port) {
    if (!Client::connect(address, port)) {
        return false;
    }
    
    // Start client thread
    m_threadRunning = true;
    m_clientThread = std::thread(&UDPClient::clientThreadFunction, this);
    
    return true;
}

bool UDPClient::connect(const core::SocketAddress& address) {
    if (!Client::connect(address)) {
        return false;
    }
    
    // Start client thread
    m_threadRunning = true;
    m_clientThread = std::thread(&UDPClient::clientThreadFunction, this);
    
    return true;
}

void UDPClient::disconnect() {
    if (!m_connected) {
        return;
    }
    
    // Stop client thread
    m_threadRunning = false;
    if (m_clientThread.joinable()) {
        m_clientThread.join();
    }
    
    Client::disconnect();
}

void UDPClient::update() {
    // UDP client doesn't need update in main thread
    // Everything is handled in the client thread
}

void UDPClient::processIncomingData(const void* data, size_t size) {
    Client::processIncomingData(data, size);
}

void UDPClient::clientThreadFunction() {
    std::vector<uint8_t> buffer(65536);
    
    while (m_threadRunning && m_connected) {
        int received = m_socket->receive(buffer.data(), buffer.size());
        
        if (received > 0) {
            handleUDPPacket(buffer.data(), received);
        } else if (received == -1) {
            // Error occurred
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void UDPClient::handleUDPPacket(const void* data, size_t size) {
    processIncomingData(data, size);
}

} // namespace client
} // namespace tienet
