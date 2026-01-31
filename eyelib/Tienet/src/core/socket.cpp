/**
 * @file socket.cpp
 * @brief Generic socket abstraction implementation
 */

#include "tienet/core/socket.hpp"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

namespace tienet {
namespace core {

// Windows socket implementation
#ifdef _WIN32

class WindowsSocket : public Socket {
public:
    WindowsSocket(SocketType type) : m_type(type), m_socket(INVALID_SOCKET), m_state(SocketState::Disconnected) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            m_state = SocketState::Error;
        }
    }
    
    virtual ~WindowsSocket() {
        close();
        WSACleanup();
    }
    
    virtual bool bind(const SocketAddress& address) override {
        if (m_socket == INVALID_SOCKET) {
            m_socket = socket(AF_INET, (m_type == SocketType::UDP) ? SOCK_DGRAM : SOCK_STREAM, 0);
            if (m_socket == INVALID_SOCKET) {
                m_state = SocketState::Error;
                return false;
            }
        }
        
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address.port);
        addr.sin_addr.s_addr = inet_addr(address.host.c_str());
        
        if (::bind(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            m_state = SocketState::Error;
            return false;
        }
        
        m_state = SocketState::Listening;
        return true;
    }
    
    virtual bool connect(const SocketAddress& address) override {
        if (m_socket == INVALID_SOCKET) {
            m_socket = socket(AF_INET, (m_type == SocketType::UDP) ? SOCK_DGRAM : SOCK_STREAM, 0);
            if (m_socket == INVALID_SOCKET) {
                m_state = SocketState::Error;
                return false;
            }
        }
        
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address.port);
        addr.sin_addr.s_addr = inet_addr(address.host.c_str());
        
        if (::connect(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            m_state = SocketState::Error;
            return false;
        }
        
        m_state = SocketState::Connected;
        return true;
    }
    
    virtual bool listen(int backlog) override {
        if (m_type != SocketType::TCP) return false;
        
        if (::listen(m_socket, backlog) == SOCKET_ERROR) {
            m_state = SocketState::Error;
            return false;
        }
        
        m_state = SocketState::Listening;
        return true;
    }
    
    virtual std::unique_ptr<Socket> accept() override {
        if (m_type != SocketType::TCP) return nullptr;
        
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = ::accept(m_socket, (sockaddr*)&clientAddr, &clientAddrLen);
        
        if (clientSocket == INVALID_SOCKET) {
            return nullptr;
        }
        
        auto client = std::make_unique<WindowsSocket>(SocketType::TCP);
        client->m_socket = clientSocket;
        client->m_state = SocketState::Connected;
        
        return std::move(client);
    }
    
    virtual int send(const void* data, size_t size) override {
        if (m_socket == INVALID_SOCKET) return -1;
        
        int result = ::send(m_socket, (const char*)data, (int)size, 0);
        if (result == SOCKET_ERROR) {
            m_state = SocketState::Error;
            return -1;
        }
        
        return result;
    }
    
    virtual int sendTo(const void* data, size_t size, const SocketAddress& address) override {
        if (m_socket == INVALID_SOCKET) return -1;
        
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address.port);
        addr.sin_addr.s_addr = inet_addr(address.host.c_str());
        
        int result = ::sendto(m_socket, (const char*)data, (int)size, 0, (sockaddr*)&addr, sizeof(addr));
        if (result == SOCKET_ERROR) {
            m_state = SocketState::Error;
            return -1;
        }
        
        return result;
    }
    
    virtual int receive(void* buffer, size_t size) override {
        if (m_socket == INVALID_SOCKET) return -1;
        
        int result = ::recv(m_socket, (char*)buffer, (int)size, 0);
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                return 0; // No data available
            }
            m_state = SocketState::Error;
            return -1;
        }
        
        if (result == 0) {
            m_state = SocketState::Disconnected;
        }
        
        return result;
    }
    
    virtual int receiveFrom(void* buffer, size_t size, SocketAddress& address) override {
        if (m_socket == INVALID_SOCKET) return -1;
        
        sockaddr_in addr;
        int addrLen = sizeof(addr);
        
        int result = ::recvfrom(m_socket, (char*)buffer, (int)size, 0, (sockaddr*)&addr, &addrLen);
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                return 0; // No data available
            }
            m_state = SocketState::Error;
            return -1;
        }
        
        address.host = inet_ntoa(addr.sin_addr);
        address.port = ntohs(addr.sin_port);
        
        return result;
    }
    
    virtual void close() override {
        if (m_socket != INVALID_SOCKET) {
            ::closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        m_state = SocketState::Disconnected;
    }
    
    virtual SocketState getState() const override {
        return m_state;
    }
    
    virtual SocketAddress getLocalAddress() const override {
        SocketAddress addr;
        if (m_socket != INVALID_SOCKET) {
            sockaddr_in localAddr;
            int addrLen = sizeof(localAddr);
            if (getsockname(m_socket, (sockaddr*)&localAddr, &addrLen) == 0) {
                addr.host = inet_ntoa(localAddr.sin_addr);
                addr.port = ntohs(localAddr.sin_port);
            }
        }
        return addr;
    }
    
    virtual SocketAddress getRemoteAddress() const override {
        SocketAddress addr;
        if (m_socket != INVALID_SOCKET) {
            sockaddr_in remoteAddr;
            int addrLen = sizeof(remoteAddr);
            if (getpeername(m_socket, (sockaddr*)&remoteAddr, &addrLen) == 0) {
                addr.host = inet_ntoa(remoteAddr.sin_addr);
                addr.port = ntohs(remoteAddr.sin_port);
            }
        }
        return addr;
    }
    
    virtual bool setOption(const std::string& option, const std::string& value) override {
        if (m_socket == INVALID_SOCKET) return false;
        
        if (option == "SO_BROADCAST") {
            BOOL broadcast = (value == "true" || value == "1");
            return setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast)) == 0;
        }
        
        return false;
    }
    
    virtual bool setNonBlocking(bool nonBlocking) override {
        if (m_socket == INVALID_SOCKET) return false;
        
        u_long mode = nonBlocking ? 1 : 0;
        return ioctlsocket(m_socket, FIONBIO, &mode) == 0;
    }
    
    virtual bool isNonBlocking() const override {
        if (m_socket == INVALID_SOCKET) return false;
        
        u_long mode;
        if (ioctlsocket(m_socket, FIONBIO, &mode) == 0) {
            return mode != 0;
        }
        
        return false;
    }

private:
    SocketType m_type;
    SOCKET m_socket;
    SocketState m_state;
};

#else

// Unix socket implementation
class UnixSocket : public Socket {
public:
    UnixSocket(SocketType type) : m_type(type), m_socket(-1), m_state(SocketState::Disconnected) {}
    
    virtual ~UnixSocket() {
        close();
    }
    
    virtual bool bind(const SocketAddress& address) override {
        if (m_socket == -1) {
            m_socket = socket(AF_INET, (m_type == SocketType::UDP) ? SOCK_DGRAM : SOCK_STREAM, 0);
            if (m_socket == -1) {
                m_state = SocketState::Error;
                return false;
            }
        }
        
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address.port);
        addr.sin_addr.s_addr = inet_addr(address.host.c_str());
        
        if (::bind(m_socket, (sockaddr*)&addr, sizeof(addr)) == -1) {
            m_state = SocketState::Error;
            return false;
        }
        
        m_state = SocketState::Listening;
        return true;
    }
    
    virtual bool connect(const SocketAddress& address) override {
        if (m_socket == -1) {
            m_socket = socket(AF_INET, (m_type == SocketType::UDP) ? SOCK_DGRAM : SOCK_STREAM, 0);
            if (m_socket == -1) {
                m_state = SocketState::Error;
                return false;
            }
        }
        
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address.port);
        addr.sin_addr.s_addr = inet_addr(address.host.c_str());
        
        if (::connect(m_socket, (sockaddr*)&addr, sizeof(addr)) == -1) {
            m_state = SocketState::Error;
            return false;
        }
        
        m_state = SocketState::Connected;
        return true;
    }
    
    virtual bool listen(int backlog) override {
        if (m_type != SocketType::TCP) return false;
        
        if (::listen(m_socket, backlog) == -1) {
            m_state = SocketState::Error;
            return false;
        }
        
        m_state = SocketState::Listening;
        return true;
    }
    
    virtual std::unique_ptr<Socket> accept() override {
        if (m_type != SocketType::TCP) return nullptr;
        
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = ::accept(m_socket, (sockaddr*)&clientAddr, &clientAddrLen);
        
        if (clientSocket == -1) {
            return nullptr;
        }
        
        auto client = std::make_unique<UnixSocket>(SocketType::TCP);
        client->m_socket = clientSocket;
        client->m_state = SocketState::Connected;
        
        return std::move(client);
    }
    
    virtual int send(const void* data, size_t size) override {
        if (m_socket == -1) return -1;
        
        ssize_t result = ::send(m_socket, data, size, 0);
        if (result == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return 0; // No data available
            }
            m_state = SocketState::Error;
            return -1;
        }
        
        return static_cast<int>(result);
    }
    
    virtual int sendTo(const void* data, size_t size, const SocketAddress& address) override {
        if (m_socket == -1) return -1;
        
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(address.port);
        addr.sin_addr.s_addr = inet_addr(address.host.c_str());
        
        ssize_t result = ::sendto(m_socket, data, size, 0, (sockaddr*)&addr, sizeof(addr));
        if (result == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return 0; // No data available
            }
            m_state = SocketState::Error;
            return -1;
        }
        
        return static_cast<int>(result);
    }
    
    virtual int receive(void* buffer, size_t size) override {
        if (m_socket == -1) return -1;
        
        ssize_t result = ::recv(m_socket, buffer, size, 0);
        if (result == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return 0; // No data available
            }
            m_state = SocketState::Error;
            return -1;
        }
        
        if (result == 0) {
            m_state = SocketState::Disconnected;
        }
        
        return static_cast<int>(result);
    }
    
    virtual int receiveFrom(void* buffer, size_t size, SocketAddress& address) override {
        if (m_socket == -1) return -1;
        
        sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        
        ssize_t result = ::recvfrom(m_socket, buffer, size, 0, (sockaddr*)&addr, &addrLen);
        if (result == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return 0; // No data available
            }
            m_state = SocketState::Error;
            return -1;
        }
        
        address.host = inet_ntoa(addr.sin_addr);
        address.port = ntohs(addr.sin_port);
        
        return static_cast<int>(result);
    }
    
    virtual void close() override {
        if (m_socket != -1) {
            ::close(m_socket);
            m_socket = -1;
        }
        m_state = SocketState::Disconnected;
    }
    
    virtual SocketState getState() const override {
        return m_state;
    }
    
    virtual SocketAddress getLocalAddress() const override {
        SocketAddress addr;
        if (m_socket != -1) {
            sockaddr_in localAddr;
            socklen_t addrLen = sizeof(localAddr);
            if (getsockname(m_socket, (sockaddr*)&localAddr, &addrLen) == 0) {
                addr.host = inet_ntoa(localAddr.sin_addr);
                addr.port = ntohs(localAddr.sin_port);
            }
        }
        return addr;
    }
    
    virtual SocketAddress getRemoteAddress() const override {
        SocketAddress addr;
        if (m_socket != -1) {
            sockaddr_in remoteAddr;
            socklen_t addrLen = sizeof(remoteAddr);
            if (getpeername(m_socket, (sockaddr*)&remoteAddr, &addrLen) == 0) {
                addr.host = inet_ntoa(remoteAddr.sin_addr);
                addr.port = ntohs(remoteAddr.sin_port);
            }
        }
        return addr;
    }
    
    virtual bool setOption(const std::string& option, const std::string& value) override {
        if (m_socket == -1) return false;
        
        if (option == "SO_BROADCAST") {
            int broadcast = (value == "true" || value == "1") ? 1 : 0;
            return setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == 0;
        }
        
        return false;
    }
    
    virtual bool setNonBlocking(bool nonBlocking) override {
        if (m_socket == -1) return false;
        
        int flags = fcntl(m_socket, F_GETFL, 0);
        if (flags == -1) return false;
        
        if (nonBlocking) {
            flags |= O_NONBLOCK;
        } else {
            flags &= ~O_NONBLOCK;
        }
        
        return fcntl(m_socket, F_SETFL, flags) == 0;
    }
    
    virtual bool isNonBlocking() const override {
        if (m_socket == -1) return false;
        
        int flags = fcntl(m_socket, F_GETFL, 0);
        if (flags == -1) return false;
        
        return (flags & O_NONBLOCK) != 0;
    }

private:
    SocketType m_type;
    int m_socket;
    SocketState m_state;
};

#endif

// SocketFactory implementation
std::unique_ptr<Socket> SocketFactory::createSocket(SocketType type) {
#ifdef _WIN32
    return std::make_unique<WindowsSocket>(type);
#else
    return std::make_unique<UnixSocket>(type);
#endif
}

std::unique_ptr<Socket> SocketFactory::createUDPSocket() {
    return createSocket(SocketType::UDP);
}

std::unique_ptr<Socket> SocketFactory::createTCPSocket() {
    return createSocket(SocketType::TCP);
}

} // namespace core
} // namespace tienet
