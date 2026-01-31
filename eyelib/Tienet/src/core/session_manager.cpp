/**
 * @file session_manager.cpp
 * @brief Session management implementation
 */

#include "tienet/core/session_manager.hpp"
#include <iostream>
#include <algorithm>

namespace tienet {
namespace core {

// Session implementation
Session::Session(uint32_t sessionId, std::shared_ptr<Socket> socket, const SessionConfig& config)
    : m_sessionId(sessionId), m_socket(socket), m_config(config), m_state(SessionState::Disconnected),
      m_heartbeatFailures(0) {
    m_connectionTime = std::chrono::steady_clock::now();
    m_lastHeartbeat = m_connectionTime;
    m_lastActivity = m_connectionTime;
}

Session::~Session() {
    shutdown();
}

bool Session::initialize() {
    if (!m_socket) {
        return false;
    }
    
    // Create protocol instance
    m_protocol = std::make_unique<SimpleUDPProtocol>();
    if (!m_protocol->initialize()) {
        return false;
    }
    
    m_protocol->setSocket(m_socket);
    m_protocol->setEventCallback([this](ProtocolEvent event, const Packet& packet) {
        handleProtocolEvent(event, packet);
    });
    m_protocol->setDataCallback([this](const Packet& packet) {
        handleProtocolData(packet);
    });
    
    m_state = SessionState::Connected;
    m_lastActivity = std::chrono::steady_clock::now();
    
    if (m_eventCallback) {
        m_eventCallback(SessionEvent::Connected, Packet());
    }
    
    return true;
}

void Session::shutdown() {
    if (m_state == SessionState::Disconnected) {
        return;
    }
    
    m_state = SessionState::Disconnecting;
    
    if (m_protocol) {
        m_protocol->disconnect();
        m_protocol.reset();
    }
    
    if (m_socket) {
        m_socket->close();
        m_socket.reset();
    }
    
    m_state = SessionState::Disconnected;
    
    if (m_eventCallback) {
        m_eventCallback(SessionEvent::Disconnected, Packet());
    }
}

void Session::update() {
    if (m_state != SessionState::Connected || !m_protocol) {
        return;
    }
    
    // Update protocol
    m_protocol->update();
    
    // Check heartbeat
    if (m_config.enableHeartbeat) {
        checkHeartbeat();
    }
    
    // Update statistics
    updateStats();
}

bool Session::sendPacket(const Packet& packet) {
    if (m_state != SessionState::Connected || !m_protocol) {
        return false;
    }
    
    bool success = m_protocol->sendPacket(packet);
    if (success) {
        m_lastActivity = std::chrono::steady_clock::now();
    }
    
    return success;
}

bool Session::sendData(const void* data, size_t size, PacketType type) {
    Packet packet(type, data, size);
    return sendPacket(packet);
}

SessionStats Session::getStats() const {
    SessionStats stats = m_stats;
    
    if (m_protocol) {
        ProtocolStats protocolStats = m_protocol->getStats();
        stats.packetsSent = protocolStats.packetsSent;
        stats.packetsReceived = protocolStats.packetsReceived;
        stats.bytesSent = protocolStats.bytesSent;
        stats.bytesReceived = protocolStats.bytesReceived;
        stats.averageLatency = protocolStats.averageLatency;
    }
    
    // Calculate connection duration
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_connectionTime);
    stats.connectionDurationMs = static_cast<uint32_t>(duration.count());
    stats.heartbeatFailures = m_heartbeatFailures;
    
    return stats;
}

SocketAddress Session::getRemoteAddress() const {
    if (m_socket) {
        return m_socket->getRemoteAddress();
    }
    return SocketAddress();
}

void Session::setEventCallback(EventCallback callback) {
    m_eventCallback = callback;
}

void Session::setDataCallback(DataCallback callback) {
    m_dataCallback = callback;
}

bool Session::isAlive() const {
    if (m_state != SessionState::Connected) {
        return false;
    }
    
    if (m_config.enableHeartbeat) {
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastActivity = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastActivity);
        return timeSinceLastActivity.count() < m_config.heartbeatTimeout;
    }
    
    return true;
}

void Session::handleProtocolEvent(ProtocolEvent event, const Packet& packet) {
    switch (event) {
        case ProtocolEvent::Connected:
            m_state = SessionState::Connected;
            if (m_eventCallback) {
                m_eventCallback(SessionEvent::Connected, packet);
            }
            break;
        case ProtocolEvent::Disconnected:
            m_state = SessionState::Disconnected;
            if (m_eventCallback) {
                m_eventCallback(SessionEvent::Disconnected, packet);
            }
            break;
        case ProtocolEvent::DataReceived:
            m_lastActivity = std::chrono::steady_clock::now();
            if (m_eventCallback) {
                m_eventCallback(SessionEvent::DataReceived, packet);
            }
            break;
        case ProtocolEvent::Error:
            m_state = SessionState::Error;
            if (m_eventCallback) {
                m_eventCallback(SessionEvent::Error, packet);
            }
            break;
    }
}

void Session::handleProtocolData(const Packet& packet) {
    m_lastActivity = std::chrono::steady_clock::now();
    
    if (m_dataCallback) {
        m_dataCallback(packet);
    }
}

void Session::sendHeartbeat() {
    Packet heartbeat(PacketType::PING, nullptr, 0);
    heartbeat.addFlag(PacketFlags::RELIABLE);
    sendPacket(heartbeat);
    m_lastHeartbeat = std::chrono::steady_clock::now();
}

void Session::checkHeartbeat() {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastHeartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastHeartbeat);
    auto timeSinceLastActivity = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastActivity);
    
    // Send heartbeat if interval has passed
    if (timeSinceLastHeartbeat.count() >= m_config.heartbeatInterval) {
        sendHeartbeat();
    }
    
    // Check for heartbeat timeout
    if (timeSinceLastActivity.count() >= m_config.heartbeatTimeout) {
        m_heartbeatFailures++;
        
        if (m_heartbeatFailures >= m_config.maxHeartbeatFailures) {
            m_state = SessionState::Error;
            if (m_eventCallback) {
                m_eventCallback(SessionEvent::HeartbeatTimeout, Packet());
            }
        }
    } else {
        // Reset failure count if we received activity
        m_heartbeatFailures = 0;
    }
}

void Session::updateStats() {
    // Stats are updated in getStats() method
}

// SessionManager implementation
SessionManager::SessionManager() 
    : m_nextSessionId(1), m_running(false) {
}

SessionManager::~SessionManager() {
    shutdown();
}

bool SessionManager::initialize(const SessionConfig& config) {
    m_config = config;
    m_running = false;
    m_nextSessionId = 1;
    m_lastPersist = std::chrono::steady_clock::now();
    
    return true;
}

void SessionManager::shutdown() {
    stopListening();
    
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    for (auto& pair : m_sessions) {
        pair.second->shutdown();
    }
    m_sessions.clear();
    m_sqlite.reset();
}

bool SessionManager::startListening(uint16_t port) {
    if (m_running) {
        return false;
    }
    
    // Create listen socket
    auto listenSocket = SocketFactory::createSocket(SocketType::TCP);
    if (!listenSocket) {
        return false;
    }
    
    m_listenSocket = std::shared_ptr<Socket>(listenSocket.release());
    
    SocketAddress listenAddr("0.0.0.0", port);
    if (!m_listenSocket->bind(listenAddr)) {
        return false;
    }
    
    if (!m_listenSocket->listen(128)) {
        return false;
    }
    
    m_running = true;
    m_acceptThread = std::thread(&SessionManager::acceptLoop, this);
    
    return true;
}

void SessionManager::stopListening() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    if (m_listenSocket) {
        m_listenSocket->close();
        m_listenSocket.reset();
    }
    
    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }
}

void SessionManager::update() {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    
    // Update all sessions
    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
        auto& session = it->second;
        
        if (!session->isAlive()) {
            // Remove dead sessions
            if (m_sqlite) {
                // Persist minimal snapshot
                PersistentChannelState snap; // reuse structure for minimal session info
                snap.channelId = 0; // reserved for session meta
                snap.stats.packetsSent = session->getStats().packetsSent;
                m_sqlite->saveChannelState(static_cast<uint8_t>(it->first & 0xFF), snap);
            }
            session->shutdown();
            it = m_sessions.erase(it);
        } else {
            session->update();
            ++it;
        }
    }

    // Periodic stats snapshot
    if (m_sqlite) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastPersist).count();
        if (elapsed >= static_cast<long long>(m_persistIntervalMs)) {
            for (const auto& pair : m_sessions) {
                uint32_t sessionId = pair.first;
                const auto stats = pair.second->getStats();
                PersistentChannelState snap; snap.channelId = 0;
                snap.stats.packetsSent = stats.packetsSent;
                snap.stats.packetsReceived = stats.packetsReceived;
                snap.stats.bytesSent = stats.bytesSent;
                snap.stats.bytesReceived = stats.bytesReceived;
                snap.lastTimestamp = stats.connectionDurationMs;
                m_sqlite->saveChannelState(static_cast<uint8_t>(sessionId & 0xFF), snap);
            }
            m_lastPersist = now;
        }
    }
}

uint32_t SessionManager::createSession(std::shared_ptr<Socket> socket) {
    if (!socket) {
        return 0;
    }
    
    uint32_t sessionId = m_nextSessionId.fetch_add(1);
    
    auto session = std::make_shared<Session>(sessionId, socket, m_config);
    session->setEventCallback([this, sessionId](SessionEvent event, const Packet& packet) {
        handleSessionEvent(sessionId, event, packet);
    });
    session->setDataCallback([this, sessionId](const Packet& packet) {
        handleSessionData(sessionId, packet);
    });
    
    if (!session->initialize()) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    m_sessions[sessionId] = session;
    
    return sessionId;
}

void SessionManager::removeSession(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end()) {
        it->second->shutdown();
        m_sessions.erase(it);
    }
}

std::shared_ptr<Session> SessionManager::getSession(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end()) {
        return it->second;
    }
    
    return nullptr;
}

bool SessionManager::sendToSession(uint32_t sessionId, const Packet& packet) {
    auto session = getSession(sessionId);
    if (session) {
        return session->sendPacket(packet);
    }
    return false;
}

uint32_t SessionManager::broadcast(const Packet& packet) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    
    uint32_t sentCount = 0;
    for (auto& pair : m_sessions) {
        if (pair.second->sendPacket(packet)) {
            sentCount++;
        }
    }
    
    return sentCount;
}

std::vector<uint32_t> SessionManager::getActiveSessions() const {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    
    std::vector<uint32_t> sessionIds;
    sessionIds.reserve(m_sessions.size());
    
    for (const auto& pair : m_sessions) {
        sessionIds.push_back(pair.first);
    }
    
    return sessionIds;
}

uint32_t SessionManager::getActiveSessionCount() const {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    return static_cast<uint32_t>(m_sessions.size());
}

void SessionManager::setSessionEventCallback(SessionEventCallback callback) {
    m_sessionEventCallback = callback;
}

void SessionManager::setDataCallback(DataCallback callback) {
    m_dataCallback = callback;
}

void SessionManager::acceptLoop() {
    while (m_running) {
        if (!m_listenSocket) {
            break;
        }
        
        // Accept incoming connection
        auto clientSocket = m_listenSocket->accept();
        if (clientSocket) {
            // Convert unique_ptr to shared_ptr
            std::shared_ptr<Socket> sharedSocket = std::move(clientSocket);
            createSession(sharedSocket);
        } else {
            // Small delay to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void SessionManager::handleSessionEvent(uint32_t sessionId, SessionEvent event, const Packet& packet) {
    (void)packet;
    if (m_sessionEventCallback) {
        m_sessionEventCallback(sessionId, event, packet);
    }
    if (!m_sqlite) return;
    if (event == SessionEvent::Connected) {
        PersistentChannelState snap; snap.channelId = 0; snap.isActive = true;
        m_sqlite->saveChannelState(static_cast<uint8_t>(sessionId & 0xFF), snap);
    } else if (event == SessionEvent::Disconnected) {
        m_sqlite->removeChannelState(static_cast<uint8_t>(sessionId & 0xFF));
    }
}

void SessionManager::handleSessionData(uint32_t sessionId, const Packet& packet) {
    if (m_dataCallback) {
        m_dataCallback(sessionId, packet);
    }
    if (m_sqlite) {
        // Optionally persist last packet header per session for diagnostics
        PersistentChannelState snap; snap.channelId = 0;
        snap.lastSequence = packet.getSequence();
        m_sqlite->saveChannelState(static_cast<uint8_t>(sessionId & 0xFF), snap);
    }
}

void SessionManager::enableSQLitePersistence(const std::string& dbPath) {
    m_sqlite = std::make_unique<SQLiteChannelPersistence>();
    if (!m_sqlite->initialize(dbPath)) {
        m_sqlite.reset();
    }
}

void SessionManager::disablePersistence() {
    m_sqlite.reset();
}

} // namespace core
} // namespace tienet
