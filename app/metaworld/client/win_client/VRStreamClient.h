#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace winclient {

// Same protocol as Flutter: format 0 = JPEG, 1 = H.264.
struct Frame {
  uint8_t format{0};
  std::vector<uint8_t> data;
  uint32_t frameId{0};
  uint64_t timestampUs{0};
};

class VRStreamClient {
public:
  static constexpr uint16_t kDiscoveryPort = 27015;
  static constexpr size_t kFrameHeaderSize = 17;   // [1 format][4 frame_id][8 timestamp_us][4 size]
  static constexpr size_t kFragmentHeaderSize = 21; // + [2 chunk_idx][2 chunk_count]
  static constexpr size_t kMaxChunkPayload = 1200;

  VRStreamClient();
  ~VRStreamClient();

  // Discovery: broadcast VR_SERVER_DISCOVERY_REQUEST, parse VR_SERVER_DISCOVERY_RESPONSE:port:name
  bool discover(std::string& outHost, uint16_t& outPort, std::string& outName, int timeoutMs = 5000);

  // Connect to stream port: send VR_CLIENT_CONNECT, wait for any response (or VR_SERVER_ACK)
  bool connect(const std::string& host, uint16_t port);

  void disconnect();

  bool isConnected() const { return connected_; }
  const std::string& host() const { return host_; }
  uint16_t port() const { return port_; }

  // Call from recv thread: process one datagram, optionally produce a complete frame.
  // Returns true if a new complete frame is available (then call takeLatestFrame).
  bool feedDatagram(const uint8_t* data, size_t size);

  // Take latest decoded frame (call after feedDatagram returns true, or poll).
  bool takeLatestFrame(Frame& out);

  // Send head rotation (radians), same as Flutter: GYRO:yaw,pitch,roll
  void sendGyro(double yaw, double pitch, double roll);

  // Send VR_CLIENT_DISCONNECT
  void sendDisconnect();

  // Raw socket for recv thread (Windows: SOCKET). Do not send on it from recv thread.
  void* getSocketForRecv() const;

private:
  struct Assembly {
    uint8_t format{0};
    uint32_t frameId{0};
    uint64_t timestampUs{0};
    uint32_t frameSize{0};
    uint16_t chunkCount{0};
    std::vector<std::vector<uint8_t>> chunks;
    size_t receivedBytes{0};
    bool complete() const;
    bool build(std::vector<uint8_t>& out) const;
  };

  static uint32_t parseU32Be(const uint8_t* p);
  static uint64_t parseU64Be(const uint8_t* p);
  static uint16_t parseU16Be(const uint8_t* p);
  bool isNewerFrameId(uint32_t frameId) const;
  void cleanupOldAssemblies();

  void* socket_{nullptr}; // SOCKET on Windows
  std::string host_;
  uint16_t port_{0};
  bool connected_{false};
  int lastEmittedFrameId_{-1};
  std::mutex mutex_;
  std::vector<std::pair<uint32_t, Assembly>> assemblies_;
  Frame latestFrame_;
  bool hasLatest_{false};
};

} // namespace winclient
