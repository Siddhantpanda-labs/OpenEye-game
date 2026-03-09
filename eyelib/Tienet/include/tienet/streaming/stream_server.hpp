/**
 * @file stream_server.hpp
 * @brief UDP-based VR stream server: discovery (27015), stream port, frame payload send, gyro callback.
 * Protocol: VR_SERVER_DISCOVERY_REQUEST/RESPONSE, VR_CLIENT_CONNECT, VR_SERVER_ACK, GYRO:yaw,pitch,roll[,timestamp_us].
 * Frame payload: [1 format][4 frame_id][8 timestamp_us][4 size][2 chunk_idx][2 chunk_count][payload]; chunk payload <= 1200.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace tienet {
namespace streaming {

/**
 * Server that binds discovery (27015) and stream (configurable) UDP ports.
 * Tick() receives control (connect, GYRO); SendFramePayload() fragments and sends to all clients.
 */
class StreamServer {
public:
    using GyroCallback = std::function<void(float yaw, float pitch, float roll, uint64_t timestamp_us)>;

    StreamServer();
    ~StreamServer();

    StreamServer(const StreamServer&) = delete;
    StreamServer& operator=(const StreamServer&) = delete;

    /** Bind discovery (27015) and stream port. Returns false if bind fails. */
    bool start(uint16_t streamPort = 27016);
    void stop();

    bool isRunning() const { return m_running; }
    uint16_t getStreamPort() const { return m_streamPort; }

    /** Call each frame. Receives UDP, handles connect/ack, discovery response, GYRO (invokes callback). */
    void tick();

    /**
     * Fragment and send payload to all connected clients. Thread-safe.
     * format: 0 = JPEG, 1 = H.264 (opaque to server).
     */
    void sendFramePayload(uint8_t format, const uint8_t* data, size_t size);

    void setGyroCallback(GyroCallback cb) { m_gyroCallback = std::move(cb); }
    void setDiscoveryServiceName(std::string name) { m_discoveryServiceName = std::move(name); }

    size_t getClientCount() const;
    float getStreamingFPS() const;

    static std::string getLocalIPv4Address();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_running = false;
    uint16_t m_streamPort = 0;
    GyroCallback m_gyroCallback;
    std::string m_discoveryServiceName = "OpenEye";
};

} // namespace streaming
} // namespace tienet
