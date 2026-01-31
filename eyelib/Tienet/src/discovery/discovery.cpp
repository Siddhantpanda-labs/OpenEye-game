/**
 * @file discovery.cpp
 */

#include "tienet/discovery/discovery.hpp"
#include "tienet/core/packet.hpp"
#include <cstring>
#include <chrono>

namespace tienet {
namespace discovery {

// Simple line format: "VR_SERVER_DISCOVERY_RESPONSE:<servicePort>:<serviceName>:<capabilities>"
static std::string makeResponse(uint16_t port, const std::string& name) {
    // For now, advertise basic capabilities
    std::string caps = "h264,nvenc;fec=on;pacing=on;abr=on";
    return std::string("VR_SERVER_DISCOVERY_RESPONSE:") + std::to_string(port) + ":" + name + ":" + caps;
}

static bool parseResponse(const std::string& s, uint16_t& outPort, std::string& outName, std::string& outCaps) {
    const std::string prefix = "VR_SERVER_DISCOVERY_RESPONSE:";
    if (s.rfind(prefix, 0) != 0) return false;
    auto rest = s.substr(prefix.size());
    auto colon1 = rest.find(':');
    if (colon1 == std::string::npos) return false;
    auto colon2 = rest.find(':', colon1 + 1);
    std::string portStr = rest.substr(0, colon1);
    outPort = static_cast<uint16_t>(std::stoi(portStr));
    if (colon2 == std::string::npos) {
        outName = rest.substr(colon1 + 1);
        outCaps.clear();
    } else {
        outName = rest.substr(colon1 + 1, colon2 - (colon1 + 1));
        outCaps = rest.substr(colon2 + 1);
    }
    return true;
}

DiscoveryServer::DiscoveryServer() : m_running(false) {}

DiscoveryServer::~DiscoveryServer() {
    shutdown();
}

bool DiscoveryServer::initialize(const DiscoveryServerConfig& config) {
    m_config = config;
    m_socket = core::SocketFactory::createUDPSocket();
    if (!m_socket) return false;
    m_socket->setOption("SO_BROADCAST", "true");
    m_socket->setNonBlocking(true);
    return m_socket->bind(core::SocketAddress(m_config.bindAddress, m_config.discoveryPort));
}

bool DiscoveryServer::start() {
    if (m_running) return true;
    m_running = true;
    m_thread = std::thread(&DiscoveryServer::serverThread, this);
    return true;
}

void DiscoveryServer::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}

void DiscoveryServer::shutdown() {
    stop();
    m_socket.reset();
    // No explicit networking shutdown required via SocketFactory
}

void DiscoveryServer::serverThread() {
    std::vector<uint8_t> buffer(2048);
    while (m_running) {
        core::SocketAddress from;
        int received = m_socket->receiveFrom(buffer.data(), buffer.size(), from);
        if (received > 0) {
            handleRequest(buffer, static_cast<size_t>(received), from);
        } else if (received == -1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void DiscoveryServer::handleRequest(const std::vector<uint8_t>& data, size_t size, const core::SocketAddress& from) {
    std::string message(reinterpret_cast<const char*>(data.data()), size);
    if (message.rfind("VR_SERVER_DISCOVERY_REQUEST", 0) == 0) {
        std::string response = makeResponse(m_config.servicePort, m_config.serviceName);
        m_socket->sendTo(response.data(), response.size(), from);
    }
}

DiscoveryClient::DiscoveryClient() : m_discoveryPort(27015) {}

bool DiscoveryClient::initialize(uint16_t discoveryPort) {
    m_discoveryPort = discoveryPort;
    m_socket = core::SocketFactory::createUDPSocket();
    if (!m_socket) return false;
    m_socket->setOption("SO_BROADCAST", "true");
    m_socket->setNonBlocking(true);
    // Bind ephemeral port
    return m_socket->bind(core::SocketAddress("0.0.0.0", 0));
}

std::vector<DiscoveryInfo> DiscoveryClient::discover(uint32_t timeoutMs, const std::string& broadcastAddr) {
    std::vector<DiscoveryInfo> results;
    if (!m_socket) return results;
    std::string req = "VR_SERVER_DISCOVERY_REQUEST";
    core::SocketAddress bcast(broadcastAddr, m_discoveryPort);
    m_socket->sendTo(req.data(), req.size(), bcast);

    auto start = std::chrono::steady_clock::now();
    std::vector<uint8_t> buffer(2048);

    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < timeoutMs) {
        core::SocketAddress from;
        int received = m_socket->receiveFrom(buffer.data(), buffer.size(), from);
        if (received > 0) {
            std::string msg(reinterpret_cast<const char*>(buffer.data()), received);
            uint16_t servicePort; std::string serviceName; std::string caps;
            if (parseResponse(msg, servicePort, serviceName, caps)) {
                DiscoveryInfo info; info.fromAddress = from; info.servicePort = servicePort; info.serviceName = serviceName; info.capabilities = caps;
                results.push_back(info);
            }
        } else if (received == -1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    return results;
}

} // namespace discovery
} // namespace tienet


