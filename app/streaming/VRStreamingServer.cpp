#include "VRStreamingServer.h"
#include "FrameEncoder.h"

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
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>

namespace eyelib {
namespace app {

struct VRStreamingServer::Impl {
    static constexpr uint16_t kDiscoveryPort = 27015;
    static constexpr size_t kMaxPacketSize = 65507u;  // UDP practical max
    static constexpr size_t kMaxFramePayload = 60000u;  // Leave room for header

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
    std::mutex clientsMutex;

    std::vector<uint8_t> sendBuffer;
    std::mutex sendMutex;
};

VRStreamingServer::VRStreamingServer() : m_impl(std::make_unique<Impl>()) {}

VRStreamingServer::~VRStreamingServer() {
    Stop();
}

bool VRStreamingServer::Start(uint16_t streamPort) {
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

void VRStreamingServer::Stop() {
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

void VRStreamingServer::Tick() {
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

    // Receive on stream port (connects + control)
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
                }
            } else {
                // Parse control: GYRO:yaw,pitch,roll or head_tracking (Flutter map toString)
                float yaw = 0.f, pitch = 0.f, roll = 0.f;
                bool gotGyro = false;
                if (msg.size() >= 5 && msg.compare(0, 5, "GYRO:") == 0) {
                    float y = 0, p = 0, r = 0;
                    if (sscanf(msg.c_str() + 5, "%f,%f,%f", &y, &p, &r) >= 3) {
                        yaw = y; pitch = p; roll = r;
                        gotGyro = true;
                    }
                } else {
                    // Flutter sends map like "{type: head_tracking, yaw: 0.1, pitch: 0.2, roll: 0.3}"
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

    // Receive on discovery port
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
            std::string reply = "VR_SERVER_DISCOVERY_RESPONSE:" + std::to_string(m_streamPort) + ":OpenEye";
            sendto(m_impl->discoverySocket, reply.data(), static_cast<int>(reply.size()), 0,
                   reinterpret_cast<struct sockaddr*>(&from), fromLen);
        }
    }
}

void VRStreamingServer::SendFrame(const uint8_t* rgba, uint32_t width, uint32_t height, int jpegQuality) {
    if (!rgba || width == 0 || height == 0) return;
    std::lock_guard<std::mutex> lock(m_impl->clientsMutex);
    if (m_impl->clients.empty()) return;

    std::lock_guard<std::mutex> sendLock(m_impl->sendMutex);
    m_impl->sendBuffer.clear();

    const uint32_t kMaxWidth = 960u;
    const uint32_t kMaxHeight = 540u;
    const uint8_t* encodePtr = rgba;
    uint32_t encodeW = width;
    uint32_t encodeH = height;
    std::vector<uint8_t> downscaled;
    if (width > kMaxWidth || height > kMaxHeight) {
        uint32_t outW = (width > kMaxWidth) ? kMaxWidth : width;
        uint32_t outH = (height > kMaxHeight) ? kMaxHeight : height;
        downscaled.resize(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 4u);
        for (uint32_t y = 0; y < outH; ++y) {
            uint32_t sy = y * height / outH;
            for (uint32_t x = 0; x < outW; ++x) {
                uint32_t sx = x * width / outW;
                size_t srcIdx = (static_cast<size_t>(sy) * width + sx) * 4u;
                size_t dstIdx = (static_cast<size_t>(y) * outW + x) * 4u;
                downscaled[dstIdx + 0] = rgba[srcIdx + 0];
                downscaled[dstIdx + 1] = rgba[srcIdx + 1];
                downscaled[dstIdx + 2] = rgba[srcIdx + 2];
                downscaled[dstIdx + 3] = rgba[srcIdx + 3];
            }
        }
        encodePtr = downscaled.data();
        encodeW = outW;
        encodeH = outH;
    }

    if (!EncodeRGBA8ToJPEG(encodePtr, encodeW, encodeH, jpegQuality, m_impl->sendBuffer))
        return;
    if (m_impl->sendBuffer.size() > Impl::kMaxFramePayload)
        return;  // Still too large; skip this frame

    const uint32_t payloadSize = static_cast<uint32_t>(m_impl->sendBuffer.size());
    // Single UDP packet: [4-byte size (big-endian)][JPEG] so client can parse one datagram
    std::vector<uint8_t> packet(4u + m_impl->sendBuffer.size());
    packet[0] = (payloadSize >> 24) & 0xff;
    packet[1] = (payloadSize >> 16) & 0xff;
    packet[2] = (payloadSize >> 8) & 0xff;
    packet[3] = payloadSize & 0xff;
    std::memcpy(packet.data() + 4, m_impl->sendBuffer.data(), m_impl->sendBuffer.size());

    for (const auto& client : m_impl->clients) {
        sendto(m_impl->streamSocket, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
               reinterpret_cast<const struct sockaddr*>(&client.addr), static_cast<int>(client.addrLen));
    }
}

size_t VRStreamingServer::GetClientCount() const {
    std::lock_guard<std::mutex> lock(m_impl->clientsMutex);
    return m_impl->clients.size();
}

std::string VRStreamingServer::GetLocalIPv4Address() {
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
    remote.sin_port = htons(9);  /* discard */
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

} // namespace app
} // namespace eyelib
