/**
 * @file discovery.hpp
 * @brief UDP discovery helpers for Tienet services
 */

#pragma once

#include "tienet/core/socket.hpp"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

namespace tienet {
namespace discovery {

struct DiscoveryServerConfig {
    uint16_t discoveryPort;
    uint16_t servicePort;      // Port that clients should connect to for streaming/service
    std::string serviceName;   // Friendly server name
    std::string bindAddress;   // Typically 0.0.0.0
    
    DiscoveryServerConfig() : discoveryPort(27015), servicePort(27016), serviceName("TienetServer"), bindAddress("0.0.0.0") {}
};

struct DiscoveryInfo {
    core::SocketAddress fromAddress; // responder IP:port
    uint16_t servicePort;            // advertised service port
    std::string serviceName;         // advertised name
    std::string capabilities;        // optional capabilities string (e.g., "h264,nvenc;latency=low")
};

class DiscoveryServer {
public:
    DiscoveryServer();
    ~DiscoveryServer();

    bool initialize(const DiscoveryServerConfig& config = DiscoveryServerConfig());
    bool start();
    void stop();
    void shutdown();

private:
    DiscoveryServerConfig m_config;
    std::shared_ptr<core::Socket> m_socket;
    std::thread m_thread;
    std::atomic<bool> m_running;

    void serverThread();
    void handleRequest(const std::vector<uint8_t>& data, size_t size, const core::SocketAddress& from);
};

class DiscoveryClient {
public:
    DiscoveryClient();
    ~DiscoveryClient() = default;

    bool initialize(uint16_t discoveryPort = 27015);
    // Broadcast discovery and collect responses for timeoutMs
    std::vector<DiscoveryInfo> discover(uint32_t timeoutMs = 1000, const std::string& broadcastAddr = "255.255.255.255");

private:
    uint16_t m_discoveryPort;
    std::shared_ptr<core::Socket> m_socket;
};

} // namespace discovery
} // namespace tienet


