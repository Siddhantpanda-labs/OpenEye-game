/**
 * @file vr_extensions.hpp
 * @brief VR-specific extensions for Tienet networking
 */

#pragma once

#include "tienet/server/server.hpp"
#include "tienet/client/client.hpp"
#include "tienet/compression/compressor.hpp"
#include "tienet/media/encoder.hpp"
#include "tienet/media/decoder.hpp"
#include <vector>
#include <functional>
#include <map>
#include <chrono>

namespace tienet {
namespace vr {

/**
 * @brief VR frame data structure
 */
struct VRFrame {
    uint32_t width;
    uint32_t height;
    uint32_t format;          // Pixel format (RGBA, RGB, etc.)
    uint32_t timestamp;
    std::vector<uint8_t> data;
    
    VRFrame() : width(0), height(0), format(0), timestamp(0) {}
    VRFrame(uint32_t w, uint32_t h, uint32_t fmt) : width(w), height(h), format(fmt), timestamp(0) {}
    
    size_t getDataSize() const { return data.size(); }
    size_t getTotalSize() const { return sizeof(VRFrame) + data.size(); }
};

/**
 * @brief Compressed VR frame
 */
struct CompressedVRFrame {
    uint32_t originalWidth;
    uint32_t originalHeight;
    uint32_t format;
    uint32_t timestamp;
    uint32_t compressedSize;
    std::vector<uint8_t> compressedData;
    
    CompressedVRFrame() : originalWidth(0), originalHeight(0), format(0), 
                         timestamp(0), compressedSize(0) {}
};

/**
 * @brief Gyroscope data
 */
struct GyroscopeData {
    float x, y, z;           // Angular velocity
    float pitch, yaw, roll;  // Euler angles
    uint32_t timestamp;
    
    GyroscopeData() : x(0), y(0), z(0), pitch(0), yaw(0), roll(0), timestamp(0) {}
};

/**
 * @brief Touch data
 */
struct TouchData {
    float x, y;              // Touch coordinates
    uint32_t touchId;        // Touch identifier
    bool isPressed;          // Touch state
    uint32_t timestamp;
    
    TouchData() : x(0), y(0), touchId(0), isPressed(false), timestamp(0) {}
};

/**
 * @brief VR server configuration
 */
struct VRServerConfig {
    uint16_t port;
    uint32_t maxClients;
    uint32_t frameRate;
    uint32_t quality;
    bool enableCompression;
    std::string compressionType;
    // Media encoder
    bool enableHardwareEncoder;
    std::string encoderType; // "none", "mf", "nvenc"
    uint32_t targetBitrateBps;
    
    VRServerConfig() : port(27016), maxClients(10), frameRate(60), quality(80),
                      enableCompression(true), compressionType("lz4"),
                      enableHardwareEncoder(false), encoderType("none"), targetBitrateBps(4 * 1024 * 1024) {}
};

/**
 * @brief VR client configuration
 */
struct VRClientConfig {
    std::string serverAddress;
    uint16_t serverPort;
    uint32_t frameRate;
    bool enableCompression;
    std::string compressionType;
    // Media decoder
    bool enableHardwareDecoder;
    std::string decoderType; // "none", "system"
    
    VRClientConfig() : serverAddress("127.0.0.1"), serverPort(27016), frameRate(60),
                      enableCompression(true), compressionType("lz4"),
                      enableHardwareDecoder(false), decoderType("none") {}
};

/**
 * @brief VR server with frame streaming capabilities
 */
class VRServer : public server::Server {
public:
    using FrameCallback = std::function<void(const VRFrame& frame)>;
    using GyroscopeCallback = std::function<void(uint32_t clientId, const GyroscopeData& data)>;
    using TouchCallback = std::function<void(uint32_t clientId, const TouchData& data)>;
    
    VRServer();
    virtual ~VRServer();
    
    /**
     * @brief Initialize VR server
     * @param config VR server configuration
     * @return true if successful, false otherwise
     */
    virtual bool initialize(const VRServerConfig& config = VRServerConfig());
    
    /**
     * @brief Start VR server
     * @return true if successful, false otherwise
     */
    virtual bool start() override;
    
    /**
     * @brief Stop VR server
     */
    virtual void stop() override;
    
    /**
     * @brief Update VR server
     */
    virtual void update() override;
    
    /**
     * @brief Send VR frame to all clients
     * @param frame VR frame to send
     * @return Number of clients frame was sent to
     */
    virtual uint32_t sendFrame(const VRFrame& frame);
    
    /**
     * @brief Send VR frame to specific client
     * @param clientId Client ID
     * @param frame VR frame to send
     * @return true if successful, false otherwise
     */
    virtual bool sendFrameToClient(uint32_t clientId, const VRFrame& frame);
    
    /**
     * @brief Send compressed VR frame to all clients
     * @param frame Compressed VR frame to send
     * @return Number of clients frame was sent to
     */
    virtual uint32_t sendCompressedFrame(const CompressedVRFrame& frame);
    
    /**
     * @brief Send compressed VR frame to specific client
     * @param clientId Client ID
     * @param frame Compressed VR frame to send
     * @return true if successful, false otherwise
     */
    virtual bool sendCompressedFrameToClient(uint32_t clientId, const CompressedVRFrame& frame);
    
    /**
     * @brief Compress VR frame
     * @param frame Original frame
     * @return Compressed frame
     */
    virtual CompressedVRFrame compressFrame(const VRFrame& frame);
    
    /**
     * @brief Set frame callback
     * @param callback Frame callback function
     */
    virtual void setFrameCallback(FrameCallback callback);
    
    /**
     * @brief Set gyroscope callback
     * @param callback Gyroscope callback function
     */
    virtual void setGyroscopeCallback(GyroscopeCallback callback);
    
    /**
     * @brief Set touch callback
     * @param callback Touch callback function
     */
    virtual void setTouchCallback(TouchCallback callback);
    
    /**
     * @brief Set compression type
     * @param type Compression type ("none", "lz4", "zstd")
     */
    virtual void setCompressionType(const std::string& type);
    
    /**
     * @brief Get compression type
     * @return Compression type
     */
    virtual std::string getCompressionType() const;
    
    /**
     * @brief Enable/disable compression
     * @param enable true to enable, false to disable
     */
    virtual void setCompressionEnabled(bool enable);
    
    /**
     * @brief Check if compression is enabled
     * @return true if enabled
     */
    virtual bool isCompressionEnabled() const;

protected:
    virtual void handleClientData(uint32_t clientId, const core::Packet& packet) override;

private:
    VRServerConfig m_vrConfig;
    std::unique_ptr<compression::Compressor> m_compressor;
    std::unique_ptr<media::IEncoder> m_encoder;
    
    FrameCallback m_frameCallback;
    GyroscopeCallback m_gyroscopeCallback;
    TouchCallback m_touchCallback;
    
    void handleVRFrame(const core::Packet& packet);
    void handleGyroscopeData(uint32_t clientId, const core::Packet& packet);
    void handleTouchData(uint32_t clientId, const core::Packet& packet);
    void setupCompressor();
    void setupEncoder();
    
    // ABR hysteresis and IDR cadence
    uint32_t m_lastEncoderBitrateBps = 0;
    std::chrono::steady_clock::time_point m_lastBitrateChange;
    std::chrono::steady_clock::time_point m_lastForcedIdr;
    bool m_encoderInitialized = false;
};

/**
 * @brief VR client with frame receiving capabilities
 */
class VRClient : public client::Client {
public:
    using FrameCallback = std::function<void(const VRFrame& frame)>;
    using CompressedFrameCallback = std::function<void(const CompressedVRFrame& frame)>;
    
    VRClient();
    virtual ~VRClient();
    
    /**
     * @brief Initialize VR client
     * @param config VR client configuration
     * @return true if successful, false otherwise
     */
    virtual bool initialize(const VRClientConfig& config = VRClientConfig());
    
    /**
     * @brief Connect to VR server
     * @param address Server address
     * @param port Server port
     * @return true if successful, false otherwise
     */
    virtual bool connect(const std::string& address, uint16_t port) override;
    
    /**
     * @brief Connect to VR server
     * @param address Server address
     * @return true if successful, false otherwise
     */
    virtual bool connect(const core::SocketAddress& address) override;
    
    /**
     * @brief Update VR client
     */
    virtual void update() override;
    // Runtime networking knobs passthrough
    void setProtocolBitrate(uint32_t bps);
    void enableProtocolPacing(bool enabled);
    void enableProtocolFEC(bool enabled, uint32_t groupSize = 8);
    void setProtocolFECParity(uint32_t parityCount);
    void setProtocolFECInterleave(uint32_t stride);
    void setProtocolMTU(uint32_t mtuBytes);
    
    /**
     * @brief Send gyroscope data to server
     * @param data Gyroscope data
     * @return true if successful, false otherwise
     */
    virtual bool sendGyroscopeData(const GyroscopeData& data);
    
    /**
     * @brief Send touch data to server
     * @param data Touch data
     * @return true if successful, false otherwise
     */
    virtual bool sendTouchData(const TouchData& data);
    
    /**
     * @brief Set frame callback
     * @param callback Frame callback function
     */
    virtual void setFrameCallback(FrameCallback callback);
    
    /**
     * @brief Set compressed frame callback
     * @param callback Compressed frame callback function
     */
    virtual void setCompressedFrameCallback(CompressedFrameCallback callback);
    
    /**
     * @brief Set compression type
     * @param type Compression type ("none", "lz4", "zstd")
     */
    virtual void setCompressionType(const std::string& type);
    
    /**
     * @brief Get compression type
     * @return Compression type
     */
    virtual std::string getCompressionType() const;
    
    /**
     * @brief Enable/disable compression
     * @param enable true to enable, false to disable
     */
    virtual void setCompressionEnabled(bool enable);
    
    /**
     * @brief Check if compression is enabled
     * @return true if enabled
     */
    virtual bool isCompressionEnabled() const;
    
    /**
     * @brief Decompress VR frame
     * @param frame Compressed frame
     * @return Decompressed frame
     */
    virtual VRFrame decompressFrame(const CompressedVRFrame& frame);

protected:
    virtual void handleDataReceived(const core::Packet& packet) override;

private:
    VRClientConfig m_vrConfig;
    std::unique_ptr<compression::Compressor> m_compressor;
    std::unique_ptr<media::IDecoder> m_decoder;
    
    FrameCallback m_frameCallback;
    CompressedFrameCallback m_compressedFrameCallback;
    
    void handleVRFrame(const core::Packet& packet);
    void handleCompressedVRFrame(const core::Packet& packet);
    void setupCompressor();
    void setupDecoder();

    // Jitter/reorder buffer and playout pacing (sequence-aware, adaptive)
    uint32_t m_targetFps = 60;
    uint32_t m_jitterBufferMs = 30; // adaptive target, clamped between min/max
    uint32_t m_minJitterMs = 15;
    uint32_t m_maxJitterMs = 120;
    struct Pending { VRFrame frame; uint32_t seq; uint32_t pktTimestamp; std::chrono::steady_clock::time_point arrival; };
    std::map<uint32_t, Pending> m_reorderBySeq;
    std::chrono::steady_clock::time_point m_firstEnqueue;
    std::chrono::steady_clock::time_point m_lastPlayout;
    bool m_bufferPrimed = false;
    bool m_haveLastSeq = false;
    uint32_t m_lastSeq = 0;
    double m_jitterEstimateMs = 0.0;
    double m_transitEstimateMs = 0.0;
    void enqueueFrameForPlayout(const VRFrame& frame, uint32_t seq, uint32_t pktTimestamp);
    void playoutTick();
public:
    void setJitterBufferMs(uint32_t ms) { m_jitterBufferMs = ms; }
};

} // namespace vr
} // namespace tienet
