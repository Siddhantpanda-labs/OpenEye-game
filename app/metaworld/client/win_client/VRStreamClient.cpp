#include "VRStreamClient.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <cstring>
#include <chrono>
#include <thread>

namespace winclient {

VRStreamClient::VRStreamClient() {
#ifdef _WIN32
  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    socket_ = nullptr;
  else
    socket_ = (void*)(intptr_t)::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_ == nullptr || (SOCKET)socket_ == INVALID_SOCKET)
    socket_ = nullptr;
#endif
}

VRStreamClient::~VRStreamClient() {
  disconnect();
#ifdef _WIN32
  if (socket_ != nullptr) {
    closesocket((SOCKET)socket_);
    socket_ = nullptr;
  }
  WSACleanup();
#endif
}

uint32_t VRStreamClient::parseU32Be(const uint8_t* p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
uint64_t VRStreamClient::parseU64Be(const uint8_t* p) {
  return (uint64_t)p[0] << 56 | (uint64_t)p[1] << 48 | (uint64_t)p[2] << 40 | (uint64_t)p[3] << 32
       | (uint64_t)p[4] << 24 | (uint64_t)p[5] << 16 | (uint64_t)p[6] << 8 | p[7];
}
uint16_t VRStreamClient::parseU16Be(const uint8_t* p) {
  return (uint16_t)((p[0] << 8) | p[1]);
}

bool VRStreamClient::Assembly::complete() const {
  return chunkCount > 0 && chunks.size() == (size_t)chunkCount && receivedBytes >= frameSize;
}

bool VRStreamClient::Assembly::build(std::vector<uint8_t>& out) const {
  out.clear();
  out.reserve(frameSize);
  for (const auto& c : chunks)
    out.insert(out.end(), c.begin(), c.end());
  if (out.size() > frameSize)
    out.resize(frameSize);
  return out.size() == frameSize;
}

bool VRStreamClient::isNewerFrameId(uint32_t frameId) const {
  if (lastEmittedFrameId_ < 0) return true;
  uint32_t a = frameId & 0xFFFFFFFFu;
  uint32_t b = (uint32_t)lastEmittedFrameId_ & 0xFFFFFFFFu;
  uint32_t diff = (a - b) & 0xFFFFFFFFu;
  return diff != 0 && diff < 0x80000000u;
}

void VRStreamClient::cleanupOldAssemblies() {
  const int64_t kTimeoutMs = 250;
  auto now = std::chrono::steady_clock::now();
  auto cutoff = now - std::chrono::milliseconds(kTimeoutMs);
  // Simple: drop assemblies older than 250ms (we don't store time per assembly; drop if too many)
  if (assemblies_.size() > 20u)
    assemblies_.erase(assemblies_.begin(), assemblies_.begin() + (assemblies_.size() - 10));
}

bool VRStreamClient::discover(std::string& outHost, uint16_t& outPort, std::string& outName, int timeoutMs) {
#ifdef _WIN32
  if (socket_ == nullptr) return false;
  SOCKET s = (SOCKET)socket_;
  unsigned long nonBlock = 1;
  ioctlsocket(s, FIONBIO, &nonBlock);

  const char req[] = "VR_SERVER_DISCOVERY_REQUEST";
  struct sockaddr_in bc{};
  bc.sin_family = AF_INET;
  bc.sin_port = htons(kDiscoveryPort);
  bc.sin_addr.s_addr = INADDR_BROADCAST;
  sendto(s, req, (int)sizeof(req) - 1, 0, (struct sockaddr*)&bc, sizeof(bc));
  inet_pton(AF_INET, "255.255.255.255", &bc.sin_addr);
  sendto(s, req, (int)sizeof(req) - 1, 0, (struct sockaddr*)&bc, sizeof(bc));

  char buf[512];
  struct sockaddr_in from{};
  int fromLen = sizeof(from);
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  while (std::chrono::steady_clock::now() < deadline) {
    int n = recvfrom(s, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&from, &fromLen);
    if (n > 0) {
      buf[n] = '\0';
      std::string resp(buf);
      if (resp.find("VR_SERVER_DISCOVERY_RESPONSE:") == 0) {
        std::string rest = resp.substr(30);
        size_t colon = rest.find(':');
        if (colon != std::string::npos) {
          outPort = (uint16_t)std::stoul(rest.substr(0, colon));
          outName = rest.substr(colon + 1);
          char addr[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &from.sin_addr, addr, sizeof(addr));
          outHost = addr;
          nonBlock = 0;
          ioctlsocket(s, FIONBIO, &nonBlock);
          return true;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  nonBlock = 0;
  ioctlsocket(s, FIONBIO, &nonBlock);
#endif
  return false;
}

bool VRStreamClient::connect(const std::string& host, uint16_t port) {
#ifdef _WIN32
  if (socket_ == nullptr) return false;
  disconnect();
  host_ = host;
  port_ = port;

  const char msg[] = "VR_CLIENT_CONNECT";
  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
  sendto((SOCKET)socket_, msg, (int)sizeof(msg) - 1, 0, (struct sockaddr*)&addr, sizeof(addr));

  char buf[2048];
  struct sockaddr_in from{};
  int fromLen = sizeof(from);
  for (int i = 0; i < 100; ++i) {
    int n = recvfrom((SOCKET)socket_, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);
    if (n > 0) {
      connected_ = true;
      // Feed first packet so we don't drop the first frame chunk
      feedDatagram(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(n));
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
#endif
  return false;
}

void* VRStreamClient::getSocketForRecv() const {
#ifdef _WIN32
  return socket_;
#else
  return nullptr;
#endif
}

void VRStreamClient::disconnect() {
  sendDisconnect();
  connected_ = false;
  std::lock_guard<std::mutex> lock(mutex_);
  assemblies_.clear();
  hasLatest_ = false;
  lastEmittedFrameId_ = -1;
}

bool VRStreamClient::feedDatagram(const uint8_t* data, size_t size) {
  if (size < 4) return false;
  std::lock_guard<std::mutex> lock(mutex_);
  cleanupOldAssemblies();

  if (size >= kFragmentHeaderSize) {
    uint32_t frameSize = parseU32Be(data + 13);
    uint16_t chunkIdx = parseU16Be(data + 17);
    uint16_t chunkCount = parseU16Be(data + 19);
    if (frameSize > 0 && chunkCount > 0 && chunkIdx < chunkCount && size > kFragmentHeaderSize) {
      uint8_t format = data[0];
      uint32_t frameId = parseU32Be(data + 1);
      uint64_t timestampUs = parseU64Be(data + 5);
      if (!isNewerFrameId(frameId)) return false;

      Assembly* a = nullptr;
      for (auto& p : assemblies_) {
        if (p.first == frameId) { a = &p.second; break; }
      }
      if (a == nullptr) {
        assemblies_.emplace_back(frameId, Assembly{});
        a = &assemblies_.back().second;
        a->format = format;
        a->frameId = frameId;
        a->timestampUs = timestampUs;
        a->frameSize = frameSize;
        a->chunkCount = chunkCount;
        a->chunks.resize(chunkCount);
        a->receivedBytes = 0;
      }
      if (a->frameSize != frameSize || a->chunkCount != chunkCount || a->format != format)
        return false;
      size_t payloadLen = size - kFragmentHeaderSize;
      a->chunks[chunkIdx].assign(data + kFragmentHeaderSize, data + size);
      a->receivedBytes += payloadLen;

      if (a->complete()) {
        std::vector<uint8_t> payload;
        if (a->build(payload) && isNewerFrameId(frameId)) {
          lastEmittedFrameId_ = (int)(frameId & 0x7FFFFFFFu);
          latestFrame_.format = a->format;
          latestFrame_.data = std::move(payload);
          latestFrame_.frameId = frameId;
          latestFrame_.timestampUs = a->timestampUs;
          hasLatest_ = true;
          for (auto it = assemblies_.begin(); it != assemblies_.end(); ++it) {
            if (it->first == frameId) { assemblies_.erase(it); break; }
          }
          return true;
        }
      }
      return false;
    }
  }

  if (size >= kFrameHeaderSize) {
    uint32_t payloadSize = parseU32Be(data + 13);
    if (payloadSize > 0 && size >= kFrameHeaderSize + payloadSize) {
      uint32_t frameId = parseU32Be(data + 1);
      uint64_t timestampUs = parseU64Be(data + 5);
      if (isNewerFrameId(frameId)) {
        lastEmittedFrameId_ = (int)(frameId & 0x7FFFFFFFu);
        latestFrame_.format = data[0];
        latestFrame_.data.assign(data + kFrameHeaderSize, data + kFrameHeaderSize + payloadSize);
        latestFrame_.frameId = frameId;
        latestFrame_.timestampUs = timestampUs;
        hasLatest_ = true;
        return true;
      }
    }
  }
  return false;
}

bool VRStreamClient::takeLatestFrame(Frame& out) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!hasLatest_) return false;
  out = std::move(latestFrame_);
  hasLatest_ = false;
  return true;
}

void VRStreamClient::sendGyro(double yaw, double pitch, double roll) {
  if (!connected_ || socket_ == nullptr) return;
#ifdef _WIN32
  char msg[128];
  int len = snprintf(msg, sizeof(msg), "GYRO:%.6f,%.6f,%.6f", yaw, pitch, roll);
  if (len <= 0 || len >= (int)sizeof(msg)) return;
  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);
  sendto((SOCKET)socket_, msg, len, 0, (struct sockaddr*)&addr, sizeof(addr));
#endif
}

void VRStreamClient::sendDisconnect() {
  if (socket_ == nullptr || host_.empty()) return;
#ifdef _WIN32
  const char msg[] = "VR_CLIENT_DISCONNECT";
  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);
  sendto((SOCKET)socket_, msg, (int)sizeof(msg) - 1, 0, (struct sockaddr*)&addr, sizeof(addr));
#endif
}

} // namespace winclient
