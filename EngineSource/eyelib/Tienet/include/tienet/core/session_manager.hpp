/**
 * @file session_manager.hpp
 * @brief Session management for multi-client connections
 */

#pragma once

#include "tienet/core/protocol.hpp"
#include "tienet/core/socket.hpp"
#include "tienet/core/db_persistence.hpp"
#include <memory>
#include <unordered_map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

namespace tienet {
namespace core {

/**
 * @brief Session states
 */
enum class SessionState {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

/**
 * @brief Session events
 */
enum class SessionEvent {
    Connected,
    Disconnected,
    DataReceived,
    Error,
    HeartbeatTimeout
};

/**
 * @brief Session statistics
 */
struct SessionStats {
    uint64_t packetsSent;
    uint64_t packetsReceived;
    uint64_t bytesSent;
    uint64_t bytesReceived;
    uint32_t averageLatency;
    uint32_t connectionDurationMs;
    uint32_t heartbeatFailures;
    
    SessionStats() : packetsSent(0), packetsReceived(0), bytesSent(0), 
                     bytesReceived(0), averageLatency(0), connectionDurationMs(0),
                     heartbeatFailures(0) {}
};

/**
 * @brief Session configuration
 */
struct SessionConfig {
    uint32_t heartbeatInterval;
    uint32_t heartbeatTimeout;
    uint32_t maxHeartbeatFailures;
    uint32_t connectionTimeout;
    bool enableHeartbeat;
    bool enableAutoReconnect;
    
    SessionConfig() : heartbeatInterval(5000), heartbeatTimeout(10000),
                     maxHeartbeatFailures(3), connectionTimeout(30000),
                     enableHeartbeat(true), enableAutoReconnect(false) {}
};

/**
 * @brief Individual session representing a client connection
 */
class Session {
public:
    using EventCallback = std::function<void(SessionEvent event, const Packet& packet)>;
    using DataCallback = std::function<void(const Packet& packet)>;
    
    Session(uint32_t sessionId, std::shared_ptr<Socket> socket, const SessionConfig& config);
    ~Session();
    
    /**
     * @brief Initialize session
     * @return true if successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Shutdown session
     */
    void shutdown();
    
    /**
     * @brief Update session (process incoming data, handle heartbeats)
     */
    void update();
    
    /**
     * @brief Send packet to this session
     * @param packet Packet to send
     * @return true if successful, false otherwise
     */
    bool sendPacket(const Packet& packet);
    
    /**
     * @brief Send data to this session
     * @param data Data to send
     * @param size Data size
     * @param type Packet type
     * @return true if successful, false otherwise
     */
    bool sendData(const void* data, size_t size, PacketType type = PacketType::DATA);
    
    /**
     * @brief Get session ID
     * @return Session ID
     */
    uint32_t getSessionId() const { return m_sessionId; }
    
    /**
     * @brief Get session state
     * @return Current session state
     */
    SessionState getState() const { return m_state; }
    
    /**
     * @brief Get session statistics
     * @return Session statistics
     */
    SessionStats getStats() const;
    
    /**
     * @brief Get remote address
     * @return Remote address
     */
    SocketAddress getRemoteAddress() const;
    
    /**
     * @brief Set event callback
     * @param callback Event callback function
     */
    void setEventCallback(EventCallback callback);
    
    /**
     * @brief Set data callback
     * @param callback Data callback function
     */
    void setDataCallback(DataCallback callback);
    
    /**
     * @brief Check if session is alive
     * @return true if session is alive, false otherwise
     */
    bool isAlive() const;

private:
    uint32_t m_sessionId;
    std::shared_ptr<Socket> m_socket;
    std::unique_ptr<Protocol> m_protocol;
    SessionConfig m_config;
    SessionState m_state;
    SessionStats m_stats;
    
    std::chrono::steady_clock::time_point m_connectionTime;
    std::chrono::steady_clock::time_point m_lastHeartbeat;
    std::chrono::steady_clock::time_point m_lastActivity;
    uint32_t m_heartbeatFailures;
    
    EventCallback m_eventCallback;
    DataCallback m_dataCallback;
    
    void handleProtocolEvent(ProtocolEvent event, const Packet& packet);
    void handleProtocolData(const Packet& packet);
    void sendHeartbeat();
    void checkHeartbeat();
    void updateStats();
};

/**
 * @brief Session manager for handling multiple client connections
 */
class SessionManager {
public:
    using SessionEventCallback = std::function<void(uint32_t sessionId, SessionEvent event, const Packet& packet)>;
    using DataCallback = std::function<void(uint32_t sessionId, const Packet& packet)>;
    
    SessionManager();
    ~SessionManager();
    
    /**
     * @brief Initialize session manager
     * @param config Session configuration
     * @return true if successful, false otherwise
     */
    bool initialize(const SessionConfig& config = SessionConfig());
    
    /**
     * @brief Shutdown session manager
     */
    void shutdown();
    
    /**
     * @brief Start accepting connections on specified port
     * @param port Port to listen on
     * @return true if successful, false otherwise
     */
    bool startListening(uint16_t port);
    
    /**
     * @brief Stop accepting connections
     */
    void stopListening();
    
    /**
     * @brief Update all sessions
     */
    void update();
    
    /**
     * @brief Create a new session from an incoming connection
     * @param socket Connected socket
     * @return Session ID, 0 if failed
     */
    uint32_t createSession(std::shared_ptr<Socket> socket);
    
    /**
     * @brief Remove a session
     * @param sessionId Session ID to remove
     */
    void removeSession(uint32_t sessionId);
    
    /**
     * @brief Get session by ID
     * @param sessionId Session ID
     * @return Pointer to session, nullptr if not found
     */
    std::shared_ptr<Session> getSession(uint32_t sessionId);
    
    /**
     * @brief Send packet to specific session
     * @param sessionId Target session ID
     * @param packet Packet to send
     * @return true if successful, false otherwise
     */
    bool sendToSession(uint32_t sessionId, const Packet& packet);
    
    /**
     * @brief Broadcast packet to all sessions
     * @param packet Packet to broadcast
     * @return Number of sessions the packet was sent to
     */
    uint32_t broadcast(const Packet& packet);
    
    /**
     * @brief Get all active session IDs
     * @return Vector of active session IDs
     */
    std::vector<uint32_t> getActiveSessions() const;
    
    /**
     * @brief Get number of active sessions
     * @return Number of active sessions
     */
    uint32_t getActiveSessionCount() const;
    
    /**
     * @brief Set session event callback
     * @param callback Event callback function
     */
    void setSessionEventCallback(SessionEventCallback callback);
    
    /**
     * @brief Set data callback
     * @param callback Data callback function
     */
    void setDataCallback(DataCallback callback);

    // Persistence hooks
    void enableSQLitePersistence(const std::string& dbPath);
    void disablePersistence();
    void setPersistenceIntervalMs(uint32_t intervalMs) { m_persistIntervalMs = intervalMs; }

private:
    SessionConfig m_config;
    std::shared_ptr<Socket> m_listenSocket;
    std::unordered_map<uint32_t, std::shared_ptr<Session>> m_sessions;
    std::atomic<uint32_t> m_nextSessionId;
    std::atomic<bool> m_running;
    std::thread m_acceptThread;
    mutable std::mutex m_sessionsMutex;
    
    SessionEventCallback m_sessionEventCallback;
    DataCallback m_dataCallback;
    std::unique_ptr<SQLiteChannelPersistence> m_sqlite;
    uint32_t m_persistIntervalMs = 5000;
    std::chrono::steady_clock::time_point m_lastPersist;
    
    void acceptLoop();
    void handleSessionEvent(uint32_t sessionId, SessionEvent event, const Packet& packet);
    void handleSessionData(uint32_t sessionId, const Packet& packet);
};

} // namespace core
} // namespace tienet
