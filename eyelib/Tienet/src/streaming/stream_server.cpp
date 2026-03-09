/**
 * @file stream_server.cpp
 * @brief VR stream server: discovery + stream UDP, frame fragmentation, gyro callback.
 */

#include "tienet/streaming/stream_server.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace tienet {
namespace streaming {

struct StreamServer::Impl {
    static constexpr uint16_t kDiscoveryPort = 27015;
    static constexpr size_t kMaxPacketSize = 65507u;
    static constexpr size_t kMaxChunkPayload = 1200u;
    static constexpr size_t kFrameHeaderSize = 1u + 4u + 8u + 4u + 2u + 2u;

    uint32_t frameId = 0;

#ifdef _WIN32
    SOCKET discoverySocket = INVALID_SOCKET;
    SOCKET streamSocket = INVALID_SOCKET;
    bool wsaStarted = false;
#else
    int discoverySocket = -1;
    int streamSocket = -1;
#endif

    struct Client {
        struct sockaddr_in addr;
        socklen_t addrLen;
#ifdef _WIN32
        Client() : addrLen(sizeof(addr)) { memset(&addr, 0, sizeof(addr)); }
#else
        socklen_t addrLen = sizeof(sockaddr_in);
        Client() { memset(&addr, 0, sizeof(addr)); }
#endif
    };
    std::vector<Client> clients;
    mutable std::mutex clientsMutex;

    mutable std::mutex fpsMutex;
    uint32_t streamingFramesInPeriod = 0;
    double streamingPeriodStart = 0.0;
    float streamingLastFps = 0.0f;
};

StreamServer::StreamServer() : m_impl(std::make_unique<Impl>()) {}

StreamServer::~StreamServer() {
    stop();
}

bool StreamServer::start(uint16_t streamPort) {
    if (m_running) return true;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return false;
    m_impl->wsaStarted = true;
#endif

    m_impl->streamSocket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (m_impl->streamSocket == INVALID_SOCKET) {
        if (m_impl->wsaStarted) { WSACleanup(); m_impl->wsaStarted = false; }
        return false;
    }
    int opt = 1;
    setsockopt(m_impl->streamSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    if (m_impl->streamSocket < 0) return false;
    int opt = 1;
    setsockopt(m_impl->streamSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in streamAddr;
    memset(&streamAddr, 0, sizeof(streamAddr));
    streamAddr.sin_family = AF_INET;
    streamAddr.sin_addr.s_addr = INADDR_ANY;
    streamAddr.sin_port = htons(streamPort);
    if (bind(m_impl->streamSocket, reinterpret_cast<struct sockaddr*>(&streamAddr), sizeof(streamAddr)) != 0) {
#ifdef _WIN32
        closesocket(m_impl->streamSocket);
        m_impl->streamSocket = INVALID_SOCKET;
        if (m_impl->wsaStarted) { WSACleanup(); m_impl->wsaStarted = false; }
#else
        close(m_impl->streamSocket);
        m_impl->streamSocket = -1;
#endif
        return false;
    }

    m_impl->discoverySocket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (m_impl->discoverySocket == INVALID_SOCKET) {
        closesocket(m_impl->streamSocket);
        m_impl->streamSocket = INVALID_SOCKET;
        if (m_impl->wsaStarted) { WSACleanup(); m_impl->wsaStarted = false; }
        return false;
    }
    setsockopt(m_impl->discoverySocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    if (m_impl->discoverySocket < 0) {
        close(m_impl->streamSocket);
        m_impl->streamSocket = -1;
        return false;
    }
    int opt2 = 1;
    setsockopt(m_impl->discoverySocket, SOL_SOCKET, SO_REUSEADDR, &opt2, sizeof(opt2));
#endif
    struct sockaddr_in discAddr;
    memset(&discAddr, 0, sizeof(discAddr));
    discAddr.sin_family = AF_INET;
    discAddr.sin_addr.s_addr = INADDR_ANY;
    discAddr.sin_port = htons(Impl::kDiscoveryPort);
    if (bind(m_impl->discoverySocket, reinterpret_cast<struct sockaddr*>(&discAddr), sizeof(discAddr)) != 0) {
#ifdef _WIN32
        closesocket(m_impl->discoverySocket);
        closesocket(m_impl->streamSocket);
        m_impl->discoverySocket = INVALID_SOCKET;
        m_impl->streamSocket = INVALID_SOCKET;
        if (m_impl->wsaStarted) { WSACleanup(); m_impl->wsaStarted = false; }
#else
        close(m_impl->discoverySocket);
        close(m_impl->streamSocket);
        m_impl->discoverySocket = -1;
        m_impl->streamSocket = -1;
#endif
        return false;
    }

#ifdef _WIN32
    u_long nonBlock = 1;
    ioctlsocket(m_impl->streamSocket, FIONBIO, &nonBlock);
    ioctlsocket(m_impl->discoverySocket, FIONBIO, &nonBlock);
#else
    int flags = fcntl(m_impl->streamSocket, F_GETFL, 0);
    fcntl(m_impl->streamSocket, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(m_impl->discoverySocket, F_GETFL, 0);
    fcntl(m_impl->discoverySocket, F_SETFL, flags | O_NONBLOCK);
#endif

    m_streamPort = streamPort;
    m_running = true;
    return true;
}

void StreamServer::stop() {
    if (!m_running) return;
    m_running = false;

#ifdef _WIN32
    if (m_impl->streamSocket != INVALID_SOCKET) {
        closesocket(m_impl->streamSocket);
        m_impl->streamSocket = INVALID_SOCKET;
    }
    if (m_impl->discoverySocket != INVALID_SOCKET) {
        closesocket(m_impl->discoverySocket);
        m_impl->discoverySocket = INVALID_SOCKET;
    }
    if (m_impl->wsaStarted) {
        WSACleanup();
        m_impl->wsaStarted = false;
    }
#else
    if (m_impl->streamSocket >= 0) {
        close(m_impl->streamSocket);
        m_impl->streamSocket = -1;
    }
    if (m_impl->discoverySocket >= 0) {
        close(m_impl->discoverySocket);
        m_impl->discoverySocket = -1;
    }
#endif
    std::lock_guard<std::mutex> lock(m_impl->clientsMutex);
    m_impl->clients.clear();
}

void StreamServer::tick() {
    if (!m_running || !m_impl->streamSocket
#ifdef _WIN32
        || m_impl->streamSocket == INVALID_SOCKET
#endif
    ) return;

    char buf[1024];
    struct sockaddr_in from;
#ifdef _WIN32
    int fromLen = sizeof(from);
#else
    socklen_t fromLen = sizeof(from);
#endif
    memset(&from, 0, sizeof(from));

    for (;;) {
#ifdef _WIN32
        int n = recvfrom(m_impl->streamSocket, buf, sizeof(buf), 0,
                        reinterpret_cast<struct sockaddr*>(&from), &fromLen);
        if (n == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) break;
            break;
        }
#else
        ssize_t n = recvfrom(m_impl->streamSocket, buf, sizeof(buf), 0,
                            reinterpret_cast<struct sockaddr*>(&from), &fromLen);
        if (n <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            break;
        }
#endif
        if (n > 0) {
            std::string msg(buf, buf + static_cast<size_t>(n));
            if (msg.find("VR_CLIENT_CONNECT") == 0) {
                std::lock_guard<std::mutex> lock(m_impl->clientsMutex);
                auto it = std::find_if(m_impl->clients.begin(), m_impl->clients.end(),
                    [&from](const Impl::Client& c) {
                        return c.addr.sin_addr.s_addr == from.sin_addr.s_addr &&
                               c.addr.sin_port == from.sin_port;
                    });
                if (it == m_impl->clients.end()) {
                    Impl::Client c;
                    c.addr = from;
                    c.addrLen = fromLen;
                    m_impl->clients.push_back(c);
                    const char ack[] = "VR_SERVER_ACK";
                    sendto(m_impl->streamSocket, ack, static_cast<int>(sizeof(ack) - 1), 0,
                           reinterpret_cast<const struct sockaddr*>(&from), static_cast<int>(fromLen));
                }
            } else {
                float yaw = 0.f, pitch = 0.f, roll = 0.f;
                bool gotGyro = false;
                if (msg.size() >= 5 && msg.compare(0, 5, "GYRO:") == 0) {
                    float y = 0, p = 0, r = 0;
                    if (sscanf(msg.c_str() + 5, "%f,%f,%f", &y, &p, &r) >= 3) {
                        yaw = y; pitch = p; roll = r;
                        gotGyro = true;
                    }
                } else {
                    size_t pos = msg.find("yaw:");
                    if (pos != std::string::npos) {
                        float y = 0, p = 0, r = 0;
                        sscanf(msg.c_str() + pos + 4, "%f", &y);
                        pos = msg.find("pitch:");
                        if (pos != std::string::npos) sscanf(msg.c_str() + pos + 6, "%f", &p);
                        pos = msg.find("roll:");
                        if (pos != std::string::npos) sscanf(msg.c_str() + pos + 5, "%f", &r);
                        yaw = y; pitch = p; roll = r;
                        gotGyro = true;
                    }
                }
                if (gotGyro && m_gyroCallback)
                    m_gyroCallback(yaw, pitch, roll);
            }
        }
    }

#ifdef _WIN32
    fromLen = sizeof(from);
    int dn = recvfrom(m_impl->discoverySocket, buf, sizeof(buf), 0,
                     reinterpret_cast<struct sockaddr*>(&from), &fromLen);
    if (dn != SOCKET_ERROR && dn > 0) {
#else
    fromLen = sizeof(from);
    ssize_t dn = recvfrom(m_impl->discoverySocket, buf, sizeof(buf), 0,
                         reinterpret_cast<struct sockaddr*>(&from), &fromLen);
    if (dn > 0) {
#endif
        std::string msg(buf, buf + static_cast<size_t>(dn));
        if (msg.find("VR_SERVER_DISCOVERY_REQUEST") == 0) {
            std::string reply = "VR_SERVER_DISCOVERY_RESPONSE:" + std::to_string(m_streamPort) + ":" + m_discoveryServiceName;
            sendto(m_impl->discoverySocket, reply.data(), static_cast<int>(reply.size()), 0,
                   reinterpret_cast<struct sockaddr*>(&from), fromLen);
        }
    }
}

void StreamServer::sendFramePayload(uint8_t format, const uint8_t* data, size_t size) {
    if (!data || size == 0) return;
    std::lock_guard<std::mutex> lock(m_impl->clientsMutex);
    if (m_impl->clients.empty()) return;

    if (size > 0xFFFFFFFFu) return;
    const uint32_t payloadSize32 = static_cast<uint32_t>(size);
    const uint32_t chunkCountU32 = (payloadSize32 + Impl::kMaxChunkPayload - 1u) / Impl::kMaxChunkPayload;
    if (chunkCountU32 == 0 || chunkCountU32 > 0xFFFFu) return;
    const uint16_t chunkCount = static_cast<uint16_t>(chunkCountU32);

    const uint32_t fid = m_impl->frameId++;
    const uint64_t timestampUs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());

    for (uint16_t chunkIdx = 0; chunkIdx < chunkCount; ++chunkIdx) {
        const size_t chunkOffset = static_cast<size_t>(chunkIdx) * Impl::kMaxChunkPayload;
        const size_t chunkSize = (std::min)(Impl::kMaxChunkPayload, size - chunkOffset);
        std::vector<uint8_t> packet(Impl::kFrameHeaderSize + chunkSize);
        size_t off = 0;
        packet[off++] = format;
        packet[off++] = (fid >> 24) & 0xff;
        packet[off++] = (fid >> 16) & 0xff;
        packet[off++] = (fid >> 8) & 0xff;
        packet[off++] = fid & 0xff;
        packet[off++] = (timestampUs >> 56) & 0xff;
        packet[off++] = (timestampUs >> 48) & 0xff;
        packet[off++] = (timestampUs >> 40) & 0xff;
        packet[off++] = (timestampUs >> 32) & 0xff;
        packet[off++] = (timestampUs >> 24) & 0xff;
        packet[off++] = (timestampUs >> 16) & 0xff;
        packet[off++] = (timestampUs >> 8) & 0xff;
        packet[off++] = timestampUs & 0xff;
        packet[off++] = (payloadSize32 >> 24) & 0xff;
        packet[off++] = (payloadSize32 >> 16) & 0xff;
        packet[off++] = (payloadSize32 >> 8) & 0xff;
        packet[off++] = payloadSize32 & 0xff;
        packet[off++] = (chunkIdx >> 8) & 0xff;
        packet[off++] = chunkIdx & 0xff;
        packet[off++] = (chunkCount >> 8) & 0xff;
        packet[off++] = chunkCount & 0xff;
        std::memcpy(packet.data() + off, data + chunkOffset, chunkSize);

        for (const auto& client : m_impl->clients) {
            sendto(m_impl->streamSocket, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
                   reinterpret_cast<const struct sockaddr*>(&client.addr), static_cast<int>(client.addrLen));
        }
    }
    {
        std::lock_guard<std::mutex> lk(m_impl->fpsMutex);
        m_impl->streamingFramesInPeriod++;
    }
}

size_t StreamServer::getClientCount() const {
    std::lock_guard<std::mutex> lock(m_impl->clientsMutex);
    return m_impl->clients.size();
}

float StreamServer::getStreamingFPS() const {
    if (!m_running) return 0.0f;
    std::lock_guard<std::mutex> lock(m_impl->fpsMutex);
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (m_impl->streamingPeriodStart == 0.0) {
        m_impl->streamingPeriodStart = now;
        return 0.0f;
    }
    const double elapsed = now - m_impl->streamingPeriodStart;
    if (elapsed >= 1.0) {
        const float fps = static_cast<float>(m_impl->streamingFramesInPeriod / elapsed);
        m_impl->streamingFramesInPeriod = 0;
        m_impl->streamingPeriodStart = now;
        m_impl->streamingLastFps = fps;
        return fps;
    }
    return m_impl->streamingLastFps;
}

std::string StreamServer::getLocalIPv4Address() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return {};
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        WSACleanup();
        return {};
    }
    struct sockaddr_in remote = {};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(9);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
    if (connect(s, reinterpret_cast<struct sockaddr*>(&remote), sizeof(remote)) != 0) {
        closesocket(s);
        WSACleanup();
        return {};
    }
    struct sockaddr_in local = {};
    int len = sizeof(local);
    if (getsockname(s, reinterpret_cast<struct sockaddr*>(&local), &len) != 0) {
        closesocket(s);
        WSACleanup();
        return {};
    }
    char buf[INET_ADDRSTRLEN];
    const char* p = inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
    closesocket(s);
    WSACleanup();
    return p ? std::string(p) : std::string();
#else
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return {};
    struct sockaddr_in remote = {};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(9);
    inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
    if (connect(s, reinterpret_cast<struct sockaddr*>(&remote), sizeof(remote)) != 0) {
        close(s);
        return {};
    }
    struct sockaddr_in local = {};
    socklen_t len = sizeof(local);
    if (getsockname(s, reinterpret_cast<struct sockaddr*>(&local), &len) != 0) {
        close(s);
        return {};
    }
    char buf[INET_ADDRSTRLEN];
    const char* p = inet_ntoa(local.sin_addr);
    close(s);
    return p ? std::string(p) : std::string();
#endif
}

} // namespace streaming
} // namespace tienet
