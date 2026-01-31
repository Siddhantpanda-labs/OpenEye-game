/**
 * @file vr_server.cpp
 * @brief VR server implementation
 */

#include "tienet/vr/vr_extensions.hpp"
#include "tienet/core/packet.hpp"
#include <iostream>

namespace tienet {
namespace vr {

// VRServer implementation
VRServer::VRServer() : server::Server() {
}

VRServer::~VRServer() {
    shutdown();
}

bool VRServer::initialize(const VRServerConfig& config) {
    m_vrConfig = config;
    
    // Initialize base server
    server::ServerConfig baseConfig;
    baseConfig.port = config.port;
    baseConfig.maxClients = config.maxClients;
    baseConfig.bindAddress = "0.0.0.0";
    
    if (!server::Server::initialize(baseConfig)) {
        return false;
    }
    
    // Setup compression
    setupCompressor();
    setupEncoder();
    m_lastBitrateChange = std::chrono::steady_clock::now();
    m_lastForcedIdr = std::chrono::steady_clock::now();
    
    return true;
}

bool VRServer::start() {
    return server::Server::start();
}

void VRServer::stop() {
    server::Server::stop();
}

void VRServer::update() {
    server::Server::update();
}

uint32_t VRServer::sendFrame(const VRFrame& frame) {
    if (!m_vrConfig.enableCompression) {
        // Uncompressed frames may exceed MTU; use MTU-aware broadcast
        return server::Server::broadcast(&frame, sizeof(VRFrame) + frame.data.size(), core::PacketType::DATA);
    } else {
        // Send compressed frame
        CompressedVRFrame compressedFrame = compressFrame(frame);
        return sendCompressedFrame(compressedFrame);
    }
}

bool VRServer::sendFrameToClient(uint32_t clientId, const VRFrame& frame) {
    if (!m_vrConfig.enableCompression) {
        // Send uncompressed frame
        core::Packet packet = core::PacketBuilder()
            .type(core::PacketType::DATA)
            .data(&frame, sizeof(VRFrame) + frame.data.size())
            .build();
        
        return sendToClient(clientId, packet);
    } else {
        // Send compressed frame
        CompressedVRFrame compressedFrame = compressFrame(frame);
        return sendCompressedFrameToClient(clientId, compressedFrame);
    }
}

uint32_t VRServer::sendCompressedFrame(const CompressedVRFrame& frame) {
    // Compressed frames can still be large; use MTU-aware broadcast
    return server::Server::broadcast(&frame, sizeof(CompressedVRFrame) + frame.compressedData.size(), core::PacketType::DATA);
}

bool VRServer::sendCompressedFrameToClient(uint32_t clientId, const CompressedVRFrame& frame) {
    core::Packet packet = core::PacketBuilder()
        .type(core::PacketType::DATA)
        .data(&frame, sizeof(CompressedVRFrame) + frame.compressedData.size())
        .build();
    
    return sendToClient(clientId, packet);
}

CompressedVRFrame VRServer::compressFrame(const VRFrame& frame) {
    CompressedVRFrame compressedFrame;
    compressedFrame.originalWidth = frame.width;
    compressedFrame.originalHeight = frame.height;
    compressedFrame.format = frame.format;
    compressedFrame.timestamp = frame.timestamp;
    
    // Encoder selection with ABR hysteresis and IDR cadence
    if (m_vrConfig.enableHardwareEncoder) {
        if (!m_encoder) {
            setupEncoder();
        }
        if (m_encoder) {
            // Query ABR target
            uint32_t abr = 0;
            if (m_protocol) abr = m_protocol->getTargetBitrateBps();
            uint32_t desired = abr ? abr : m_vrConfig.targetBitrateBps;
            if (m_lastEncoderBitrateBps == 0) m_lastEncoderBitrateBps = desired;
            
            // Hysteresis: only change if >15% delta and at least 500ms since last change
            auto now = std::chrono::steady_clock::now();
            bool allowChange = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastBitrateChange).count() > 500;
            double delta = (double)std::abs((int)desired - (int)m_lastEncoderBitrateBps) / (double)m_lastEncoderBitrateBps;
            if (allowChange && delta > 0.15) {
                m_lastEncoderBitrateBps = desired;
                m_lastBitrateChange = now;
                media::EncoderConfig ec{ frame.width, frame.height, m_lastEncoderBitrateBps, m_vrConfig.frameRate, "h264", 1000 };
                if (!m_encoderInitialized) {
                    m_encoderInitialized = m_encoder->initialize(ec);
                } else {
                    m_encoder->reconfigure(ec);
                }
            } else if (!m_encoderInitialized) {
                // Initial init
                media::EncoderConfig ec{ frame.width, frame.height, m_lastEncoderBitrateBps, m_vrConfig.frameRate, "h264", 1000 };
                m_encoderInitialized = m_encoder->initialize(ec);
            }
            
            // IDR cadence: force keyframe every keyframeIntervalMs
            uint32_t intervalMs = 1000; // default 1s
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastForcedIdr).count();
            if (elapsed >= (int)intervalMs) {
                m_encoder->forceKeyframe();
                m_lastForcedIdr = now;
            }
            
            if (m_encoderInitialized) {
                compressedFrame.compressedData = m_encoder->encode(frame.data);
                compressedFrame.compressedSize = static_cast<uint32_t>(compressedFrame.compressedData.size());
                return compressedFrame;
            }
        }
    }

    if (m_compressor) {
        compressedFrame.compressedData = m_compressor->compress(frame.data, compression::CompressionLevel::Balanced);
        compressedFrame.compressedSize = static_cast<uint32_t>(compressedFrame.compressedData.size());
    } else {
        compressedFrame.compressedData = frame.data;
        compressedFrame.compressedSize = static_cast<uint32_t>(frame.data.size());
    }
    
    return compressedFrame;
}

void VRServer::setFrameCallback(FrameCallback callback) {
    m_frameCallback = callback;
}

void VRServer::setGyroscopeCallback(GyroscopeCallback callback) {
    m_gyroscopeCallback = callback;
}

void VRServer::setTouchCallback(TouchCallback callback) {
    m_touchCallback = callback;
}

void VRServer::setCompressionType(const std::string& type) {
    m_vrConfig.compressionType = type;
    setupCompressor();
}

std::string VRServer::getCompressionType() const {
    return m_vrConfig.compressionType;
}

void VRServer::setCompressionEnabled(bool enable) {
    m_vrConfig.enableCompression = enable;
}

bool VRServer::isCompressionEnabled() const {
    return m_vrConfig.enableCompression;
}

void VRServer::handleClientData(uint32_t clientId, const core::Packet& packet) {
    // Handle VR-specific packet types
    switch (packet.getType()) {
        case core::PacketType::DATA:
            // Check if it's VR frame data
            if (packet.getDataSize() >= sizeof(VRFrame)) {
                handleVRFrame(packet);
            } else if (packet.getDataSize() >= sizeof(GyroscopeData)) {
                handleGyroscopeData(clientId, packet);
            } else if (packet.getDataSize() >= sizeof(TouchData)) {
                handleTouchData(clientId, packet);
            }
            break;
        default:
            // Handle other packet types
            server::Server::handleClientData(clientId, packet);
            break;
    }
}

void VRServer::handleVRFrame(const core::Packet& packet) {
    if (packet.getDataSize() < sizeof(VRFrame)) {
        return;
    }
    
    const VRFrame* frame = reinterpret_cast<const VRFrame*>(packet.getData().data());
    
    if (m_frameCallback) {
        m_frameCallback(*frame);
    }
}

void VRServer::handleGyroscopeData(uint32_t clientId, const core::Packet& packet) {
    if (packet.getDataSize() < sizeof(GyroscopeData)) {
        return;
    }
    
    const GyroscopeData* data = reinterpret_cast<const GyroscopeData*>(packet.getData().data());
    
    if (m_gyroscopeCallback) {
        m_gyroscopeCallback(clientId, *data);
    }
}

void VRServer::handleTouchData(uint32_t clientId, const core::Packet& packet) {
    if (packet.getDataSize() < sizeof(TouchData)) {
        return;
    }
    
    const TouchData* data = reinterpret_cast<const TouchData*>(packet.getData().data());
    
    if (m_touchCallback) {
        m_touchCallback(clientId, *data);
    }
}

void VRServer::setupCompressor() {
    m_compressor = compression::CompressorFactory::createCompressor(m_vrConfig.compressionType);
    
    if (!m_compressor || !m_compressor->isAvailable()) {
        std::cout << "⚠️ Compression not available, falling back to no compression" << std::endl;
        m_compressor = compression::CompressorFactory::createNoCompressor();
    }
}

void VRServer::setupEncoder() {
    if (m_vrConfig.enableHardwareEncoder) {
        m_encoder = media::EncoderFactory::create(m_vrConfig.encoderType);
        if (!m_encoder) {
            std::cout << "⚠️ Encoder not available, falling back to software compression" << std::endl;
            m_vrConfig.enableHardwareEncoder = false;
        } else {
            m_encoderInitialized = false;
            m_lastEncoderBitrateBps = 0;
        }
    }
}

// VRClient implementation
VRClient::VRClient() : client::Client() {
}

VRClient::~VRClient() {
    shutdown();
}

bool VRClient::initialize(const VRClientConfig& config) {
    m_vrConfig = config;
    
    // Initialize base client
    client::ClientConfig baseConfig;
    baseConfig.serverAddress = config.serverAddress;
    baseConfig.serverPort = config.serverPort;
    baseConfig.autoReconnect = true;
    baseConfig.enableHeartbeat = true;
    
    if (!client::Client::initialize(baseConfig)) {
        return false;
    }
    
    // Setup compression and decoder
    setupCompressor();
    setupDecoder();
    
    return true;
}

bool VRClient::connect(const std::string& address, uint16_t port) {
    return client::Client::connect(address, port);
}

bool VRClient::connect(const core::SocketAddress& address) {
    return client::Client::connect(address);
}

void VRClient::update() {
    client::Client::update();
    playoutTick();
}

void VRClient::setProtocolBitrate(uint32_t bps) {
    Client::setProtocolBitrate(bps);
}

void VRClient::enableProtocolPacing(bool enabled) {
    Client::enableProtocolPacing(enabled);
}

void VRClient::enableProtocolFEC(bool enabled, uint32_t groupSize) {
    Client::enableProtocolFEC(enabled, groupSize);
}

void VRClient::setProtocolFECParity(uint32_t parityCount) {
    Client::setProtocolFECParity(parityCount);
}

void VRClient::setProtocolFECInterleave(uint32_t stride) {
    Client::setProtocolFECInterleave(stride);
}

void VRClient::setProtocolMTU(uint32_t mtuBytes) {
    Client::setProtocolMTU(mtuBytes);
}

bool VRClient::sendGyroscopeData(const GyroscopeData& data) {
    core::Packet packet = core::PacketBuilder()
        .type(core::PacketType::DATA)
        .data(&data, sizeof(GyroscopeData))
        .build();
    
    return sendPacket(packet);
}

bool VRClient::sendTouchData(const TouchData& data) {
    core::Packet packet = core::PacketBuilder()
        .type(core::PacketType::DATA)
        .data(&data, sizeof(TouchData))
        .build();
    
    return sendPacket(packet);
}

void VRClient::setFrameCallback(FrameCallback callback) {
    m_frameCallback = callback;
}

void VRClient::setCompressedFrameCallback(CompressedFrameCallback callback) {
    m_compressedFrameCallback = callback;
}

void VRClient::setCompressionType(const std::string& type) {
    m_vrConfig.compressionType = type;
    setupCompressor();
}

std::string VRClient::getCompressionType() const {
    return m_vrConfig.compressionType;
}

void VRClient::setCompressionEnabled(bool enable) {
    m_vrConfig.enableCompression = enable;
}

bool VRClient::isCompressionEnabled() const {
    return m_vrConfig.enableCompression;
}

VRFrame VRClient::decompressFrame(const CompressedVRFrame& frame) {
    VRFrame decompressedFrame;
    decompressedFrame.width = frame.originalWidth;
    decompressedFrame.height = frame.originalHeight;
    decompressedFrame.format = frame.format;
    decompressedFrame.timestamp = frame.timestamp;
    
    if (m_decoder && m_vrConfig.enableHardwareDecoder) {
        media::DecoderConfig dc{ frame.originalWidth, frame.originalHeight, "h264" };
        m_decoder->initialize(dc);
        decompressedFrame.data = m_decoder->decode(frame.compressedData);
    } else if (m_compressor && m_vrConfig.enableCompression) {
        decompressedFrame.data = m_compressor->decompress(frame.compressedData, frame.compressedSize);
    } else {
        decompressedFrame.data = frame.compressedData;
    }
    
    return decompressedFrame;
}

void VRClient::handleDataReceived(const core::Packet& packet) {
    // Handle VR-specific packet types
    switch (packet.getType()) {
        case core::PacketType::DATA:
            // VR data: prefer CompressedVRFrame first (larger struct) else VRFrame
            if (packet.getDataSize() >= sizeof(CompressedVRFrame)) {
                handleCompressedVRFrame(packet);
            } else if (packet.getDataSize() >= sizeof(VRFrame)) {
                handleVRFrame(packet);
            }
            break;
        default:
            // Handle other packet types
            client::Client::handleDataReceived(packet);
            break;
    }
}

void VRClient::handleVRFrame(const core::Packet& packet) {
    if (packet.getDataSize() < sizeof(VRFrame)) {
        return;
    }
    const VRFrame* frame = reinterpret_cast<const VRFrame*>(packet.getData().data());
    enqueueFrameForPlayout(*frame, packet.getSequence(), packet.getTimestamp());
}

void VRClient::handleCompressedVRFrame(const core::Packet& packet) {
    if (packet.getDataSize() < sizeof(CompressedVRFrame)) {
        return;
    }
    const CompressedVRFrame* compressedFrame = reinterpret_cast<const CompressedVRFrame*>(packet.getData().data());
    if (m_compressedFrameCallback) {
        m_compressedFrameCallback(*compressedFrame);
    }
    VRFrame decompressedFrame = decompressFrame(*compressedFrame);
    enqueueFrameForPlayout(decompressedFrame, packet.getSequence(), packet.getTimestamp());
}

void VRClient::setupCompressor() {
    m_compressor = compression::CompressorFactory::createCompressor(m_vrConfig.compressionType);
    
    if (!m_compressor || !m_compressor->isAvailable()) {
        std::cout << "⚠️ Compression not available, falling back to no compression" << std::endl;
        m_compressor = compression::CompressorFactory::createNoCompressor();
    }
}

void VRClient::setupDecoder() {
    if (m_vrConfig.enableHardwareDecoder) {
        m_decoder = media::DecoderFactory::create(m_vrConfig.decoderType);
        if (!m_decoder) {
            std::cout << "⚠️ Decoder not available, falling back to software/no compression" << std::endl;
            m_vrConfig.enableHardwareDecoder = false;
        }
    }
}

void VRClient::enqueueFrameForPlayout(const VRFrame& frame, uint32_t seq, uint32_t pktTimestamp) {
    auto now = std::chrono::steady_clock::now();
    Pending p{ frame, seq, pktTimestamp, now };
    m_reorderBySeq[seq] = std::move(p);

    // Estimate network jitter (EWMA of absolute inter-arrival variation)
    if (!m_haveLastSeq) {
        m_haveLastSeq = true;
        m_lastSeq = seq;
        m_firstEnqueue = now;
        m_bufferPrimed = false;
        m_jitterEstimateMs = 0.0;
        m_transitEstimateMs = 0.0;
        return;
    }

    if (seq > m_lastSeq) {
        double deltaSeq = static_cast<double>(seq - m_lastSeq);
        // Approximate frame period by target fps
        double frameMs = (m_targetFps > 0) ? (1000.0 / m_targetFps) : 16.0;
        double expectedMs = deltaSeq * frameMs;
        double iatMs = std::chrono::duration<double, std::milli>(now - m_lastPlayout).count();
        if (m_lastPlayout.time_since_epoch().count() == 0) {
            iatMs = std::chrono::duration<double, std::milli>(now - m_firstEnqueue).count();
        }
        double variation = std::abs(iatMs - expectedMs);
        // EWMA with alpha=0.1
        m_jitterEstimateMs = 0.9 * m_jitterEstimateMs + 0.1 * variation;
        // Simple transit estimate (can be extended with packet timestamp deltas)
        m_transitEstimateMs = 0.9 * m_transitEstimateMs + 0.1 * iatMs;
        m_lastSeq = seq;
    }

    // Adaptive buffer: target ~ 3x jitter, clamp
    uint32_t target = static_cast<uint32_t>(std::min<double>(m_maxJitterMs, std::max<double>(m_minJitterMs, 3.0 * m_jitterEstimateMs)));
    m_jitterBufferMs = target;

    if (!m_bufferPrimed) {
        auto bufferedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_firstEnqueue).count();
        if (bufferedMs >= static_cast<int64_t>(m_jitterBufferMs)) {
            m_bufferPrimed = true;
            m_lastPlayout = now;
        }
    }
}

void VRClient::playoutTick() {
    if (!m_bufferPrimed) return;
    if (m_reorderBySeq.empty()) return;
    uint32_t frameIntervalMs = (m_targetFps > 0) ? (1000 / m_targetFps) : 16;
    auto now = std::chrono::steady_clock::now();
    auto sinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastPlayout).count();
    if (sinceLast < static_cast<int64_t>(frameIntervalMs)) return;

    // Emit the lowest sequence frame (in order), drop late frames beyond buffer
    auto it = m_reorderBySeq.begin();
    VRFrame frame = it->second.frame;
    m_reorderBySeq.erase(it);
    m_lastPlayout = now;
    if (m_frameCallback) m_frameCallback(frame);
}

} // namespace vr
} // namespace tienet
