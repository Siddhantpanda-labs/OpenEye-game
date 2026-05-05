#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace app {

/**
 * Optional VR streaming server: UDP discovery + stream port.
 * Responds to VR_SERVER_DISCOVERY_REQUEST; clients connect to stream port and receive
 * JPEG frames; server receives gyro (head_tracking) and invokes callback.
 * Thread-safe for SendFrame and SetGyroCallback; run Tick() on main/render thread.
 */
class VRStreamingServer {
public:
    using GyroCallback = std::function<void(float yaw, float pitch, float roll)>;

    VRStreamingServer();
    ~VRStreamingServer();

    VRStreamingServer(const VRStreamingServer&) = delete;
    VRStreamingServer& operator=(const VRStreamingServer&) = delete;

    /** Start discovery (port 27015) and stream (port) listeners. Returns false if bind fails. */
    bool Start(uint16_t streamPort = 27016);
    void Stop();

    bool IsRunning() const { return m_running; }
    uint16_t GetStreamPort() const { return m_streamPort; }

    /** Call each frame after render. Receives UDP, parses control (GYRO/head_tracking), invokes gyro callback. */
    void Tick();

    /**
     * Encode RGBA8 (row-major, top-left origin) to H.264 and send to all connected clients.
     * Safe to call from encoder thread. No-op if no clients or H.264 encoder unavailable.
     */
    void SendFrame(const uint8_t* rgba, uint32_t width, uint32_t height, int jpegQuality = 75);

    /**
     * Encode raw NV12 (Y plane then interleaved UV) to H.264 and send. Use when GPU or other path produced NV12.
     * pitch: row stride in bytes (0 = width). Use aligned pitch (e.g. 256) when buffer is pitch-aligned for NVENC.
     * No-op if no clients or H.264 encoder not available. Safe to call from encoder thread.
     */
    void SendFrameNV12(const uint8_t* nv12, uint32_t width, uint32_t height, uint32_t pitch = 0);

    void SetGyroCallback(GyroCallback cb) { m_gyroCallback = std::move(cb); }
    /** Number of clients that have sent VR_CLIENT_CONNECT and are receiving frames. */
    size_t GetClientCount() const;

    /** Frames sent per second (rolling 1s window). 0 when not running or no frames sent yet. */
    float GetStreamingFPS() const;

    /** Get local IPv4 address for display (e.g. for QR). Returns empty string on failure. */
    static std::string GetLocalIPv4Address();

private:
    struct Impl;
    mutable std::unique_ptr<Impl> m_impl;
    bool m_running = false;
    uint16_t m_streamPort = 0;
    GyroCallback m_gyroCallback;
};

} // namespace app
} // namespace eyelib
