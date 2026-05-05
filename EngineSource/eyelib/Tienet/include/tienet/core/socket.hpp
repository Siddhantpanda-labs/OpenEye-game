/**
 * @file socket.hpp
 * @brief Generic socket abstraction for cross-platform networking
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace tienet {
namespace core {

/**
 * @brief Socket address information
 */
struct SocketAddress {
    std::string host;
    uint16_t port;
    
    SocketAddress() : port(0) {}
    SocketAddress(const std::string& h, uint16_t p) : host(h), port(p) {}
    
    std::string toString() const {
        return host + ":" + std::to_string(port);
    }
};

/**
 * @brief Socket types
 */
enum class SocketType {
    UDP,
    TCP
};

/**
 * @brief Socket states
 */
enum class SocketState {
    Disconnected,
    Connecting,
    Connected,
    Listening,
    Error
};

/**
 * @brief Abstract socket interface
 */
class Socket {
public:
    virtual ~Socket() = default;
    
    /**
     * @brief Bind socket to address and port
     * @param address Socket address
     * @return true if successful, false otherwise
     */
    virtual bool bind(const SocketAddress& address) = 0;
    
    /**
     * @brief Connect to remote address
     * @param address Remote address
     * @return true if successful, false otherwise
     */
    virtual bool connect(const SocketAddress& address) = 0;
    
    /**
     * @brief Listen for incoming connections (TCP only)
     * @param backlog Maximum number of pending connections
     * @return true if successful, false otherwise
     */
    virtual bool listen(int backlog = 5) = 0;
    
    /**
     * @brief Accept incoming connection (TCP only)
     * @return New socket for the connection, nullptr on failure
     */
    virtual std::unique_ptr<Socket> accept() = 0;
    
    /**
     * @brief Send data
     * @param data Data to send
     * @param size Size of data
     * @return Number of bytes sent, -1 on error
     */
    virtual int send(const void* data, size_t size) = 0;
    
    /**
     * @brief Send data to specific address (UDP only)
     * @param data Data to send
     * @param size Size of data
     * @param address Destination address
     * @return Number of bytes sent, -1 on error
     */
    virtual int sendTo(const void* data, size_t size, const SocketAddress& address) = 0;
    
    /**
     * @brief Receive data
     * @param buffer Buffer to store received data
     * @param size Buffer size
     * @return Number of bytes received, -1 on error
     */
    virtual int receive(void* buffer, size_t size) = 0;
    
    /**
     * @brief Receive data with sender address (UDP only)
     * @param buffer Buffer to store received data
     * @param size Buffer size
     * @param address Sender address
     * @return Number of bytes received, -1 on error
     */
    virtual int receiveFrom(void* buffer, size_t size, SocketAddress& address) = 0;
    
    /**
     * @brief Close the socket
     */
    virtual void close() = 0;
    
    /**
     * @brief Get socket state
     * @return Current socket state
     */
    virtual SocketState getState() const = 0;
    
    /**
     * @brief Get local address
     * @return Local socket address
     */
    virtual SocketAddress getLocalAddress() const = 0;
    
    /**
     * @brief Get remote address
     * @return Remote socket address
     */
    virtual SocketAddress getRemoteAddress() const = 0;
    
    /**
     * @brief Set socket option
     * @param option Option name
     * @param value Option value
     * @return true if successful, false otherwise
     */
    virtual bool setOption(const std::string& option, const std::string& value) = 0;
    
    /**
     * @brief Set non-blocking mode
     * @param nonBlocking true for non-blocking, false for blocking
     * @return true if successful, false otherwise
     */
    virtual bool setNonBlocking(bool nonBlocking) = 0;
    
    /**
     * @brief Check if socket is non-blocking
     * @return true if non-blocking, false if blocking
     */
    virtual bool isNonBlocking() const = 0;
};

/**
 * @brief Socket factory
 */
class SocketFactory {
public:
    /**
     * @brief Create a new socket
     * @param type Socket type (UDP/TCP)
     * @return New socket instance
     */
    static std::unique_ptr<Socket> createSocket(SocketType type);
    
    /**
     * @brief Create UDP socket
     * @return New UDP socket
     */
    static std::unique_ptr<Socket> createUDPSocket();
    
    /**
     * @brief Create TCP socket
     * @return New TCP socket
     */
    static std::unique_ptr<Socket> createTCPSocket();
};

} // namespace core
} // namespace tienet
