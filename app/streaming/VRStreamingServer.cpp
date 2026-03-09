#include "VRStreamingServer.h"
#include <tienet/streaming/stream_server.hpp>

#if defined(OPENEYE_STREAMING_H264) && OPENEYE_STREAMING_H264
#include <tienet/media/encoder.hpp>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace eyelib {
namespace app {

struct VRStreamingServer::Impl {
    static constexpr uint8_t kFormatH264 = 1;

    std::unique_ptr<tienet::streaming::StreamServer> streamServer;
    std::vector<uint8_t> sendBuffer;

#if defined(OPENEYE_STREAMING_H264) && OPENEYE_STREAMING_H264
    std::unique_ptr<tienet::media::IEncoder> h264Encoder;
    std::vector<uint8_t> rgbaEncodeBuffer;
    uint32_t encoderW = 0;
    uint32_t encoderH = 0;
    static constexpr uint32_t kTargetStreamFps = 90u;
    static constexpr uint32_t kStreamBitrateBps = 4u * 1024u * 1024u;
    uint32_t nv12EmptyCount = 0;
    bool nv12FirstSent = false;
    static constexpr uint32_t kGopFrames = 15u;
    std::chrono::steady_clock::time_point nextSendTime{};
    static constexpr double kFrameIntervalSec = 1.0 / static_cast<double>(kTargetStreamFps);
#endif
};

VRStreamingServer::VRStreamingServer() : m_impl(std::make_unique<Impl>()) {
    m_impl->streamServer = std::make_unique<tienet::streaming::StreamServer>();
}

VRStreamingServer::~VRStreamingServer() {
    Stop();
}

bool VRStreamingServer::Start(uint16_t streamPort) {
    if (m_running) return true;
    if (!m_impl->streamServer->start(streamPort)) return false;
    m_impl->streamServer->setGyroCallback([this](float yaw, float pitch, float roll, uint64_t /*timestamp_us*/) {
        if (m_gyroCallback) m_gyroCallback(yaw, pitch, roll);
    });
    m_impl->streamServer->setDiscoveryServiceName("OpenEye");
    m_streamPort = streamPort;
#if defined(OPENEYE_STREAMING_H264) && OPENEYE_STREAMING_H264
    m_impl->nextSendTime = {};
#endif
    m_running = true;
    return true;
}

void VRStreamingServer::Stop() {
    if (!m_running) return;
    m_running = false;
    m_impl->streamServer->stop();
#if defined(OPENEYE_STREAMING_H264) && OPENEYE_STREAMING_H264
    if (m_impl->h264Encoder) {
        m_impl->h264Encoder->shutdown();
        m_impl->h264Encoder.reset();
    }
#endif
}

void VRStreamingServer::Tick() {
    if (!m_running) return;
    m_impl->streamServer->tick();
}

void VRStreamingServer::SendFrame(const uint8_t* rgba, uint32_t width, uint32_t height, int jpegQuality) {
    if (!rgba || width == 0 || height == 0) return;
#if defined(OPENEYE_STREAMING_H264) && OPENEYE_STREAMING_H264
    {
        const auto now = std::chrono::steady_clock::now();
        if (m_impl->nextSendTime == std::chrono::steady_clock::time_point{})
            m_impl->nextSendTime = now;
        if (now < m_impl->nextSendTime)
            return;
    }
#endif
    if (m_impl->streamServer->getClientCount() == 0) return;

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

    uint8_t formatByte = Impl::kFormatH264;
    const uint8_t* payloadPtr = nullptr;
    size_t payloadSize = 0;

#if defined(OPENEYE_STREAMING_H264) && OPENEYE_STREAMING_H264 && defined(_WIN32)
    if (!m_impl->h264Encoder) {
        tienet::media::EncoderConfig cfg;
        cfg.width = encodeW;
        cfg.height = encodeH;
        cfg.bitrateBps = Impl::kStreamBitrateBps;
        cfg.fps = Impl::kTargetStreamFps;
        cfg.codec = "h264";
        cfg.keyframeIntervalMs = (Impl::kGopFrames > 0u) ? (1000u * Impl::kGopFrames / Impl::kTargetStreamFps) : 1000u;
#ifdef TIENET_HAS_NVENC
        std::fprintf(stderr, "[VRStreaming] Using H264 encoder: nvenc (RGBA path), stream %u FPS\n", Impl::kTargetStreamFps);
        m_impl->h264Encoder = tienet::media::EncoderFactory::create("nvenc");
#else
        std::fprintf(stderr, "[VRStreaming] Using H264 encoder: mf (RGBA path)\n");
        m_impl->h264Encoder = tienet::media::EncoderFactory::create("mf");
#endif
        if (m_impl->h264Encoder && m_impl->h264Encoder->initialize(cfg)) {
            m_impl->encoderW = encodeW;
            m_impl->encoderH = encodeH;
        } else {
            std::fprintf(stderr, "[VRStreaming] H264 encoder init failed (RGBA path), encoder discarded\n");
            m_impl->h264Encoder.reset();
        }
    }
    if (m_impl->h264Encoder && (encodeW != m_impl->encoderW || encodeH != m_impl->encoderH)) {
        tienet::media::EncoderConfig cfg;
        cfg.width = encodeW;
        cfg.height = encodeH;
        cfg.bitrateBps = Impl::kStreamBitrateBps;
        cfg.fps = Impl::kTargetStreamFps;
        cfg.codec = "h264";
        cfg.keyframeIntervalMs = (Impl::kGopFrames > 0u) ? (1000u * Impl::kGopFrames / Impl::kTargetStreamFps) : 1000u;
        if (m_impl->h264Encoder->reconfigure(cfg)) {
            m_impl->encoderW = encodeW;
            m_impl->encoderH = encodeH;
        }
    }
    if (m_impl->h264Encoder) {
        m_impl->rgbaEncodeBuffer.assign(encodePtr, encodePtr + static_cast<size_t>(encodeW) * static_cast<size_t>(encodeH) * 4u);
        std::vector<uint8_t> h264 = m_impl->h264Encoder->encode(m_impl->rgbaEncodeBuffer);
        if (!h264.empty()) {
            m_impl->sendBuffer = std::move(h264);
            formatByte = Impl::kFormatH264;
            payloadPtr = m_impl->sendBuffer.data();
            payloadSize = m_impl->sendBuffer.size();
        } else {
            std::fprintf(stderr, "[VRStreaming] encode returned empty (RGBA path)\n");
        }
    }
#endif

    if (payloadPtr == nullptr || payloadSize == 0 || payloadSize > 0xFFFFFFFFu) return;
    m_impl->streamServer->sendFramePayload(formatByte, payloadPtr, payloadSize);

#if defined(OPENEYE_STREAMING_H264) && OPENEYE_STREAMING_H264
    {
        const auto now = std::chrono::steady_clock::now();
        m_impl->nextSendTime += std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(Impl::kFrameIntervalSec));
        if (m_impl->nextSendTime < now)
            m_impl->nextSendTime = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(Impl::kFrameIntervalSec));
    }
#endif
}

void VRStreamingServer::SendFrameNV12(const uint8_t* nv12, uint32_t width, uint32_t height, uint32_t pitch) {
    if (!nv12 || width == 0 || height == 0) return;
#if defined(OPENEYE_STREAMING_H264) && OPENEYE_STREAMING_H264
    {
        const auto now = std::chrono::steady_clock::now();
        if (m_impl->nextSendTime == std::chrono::steady_clock::time_point{})
            m_impl->nextSendTime = now;
        if (now < m_impl->nextSendTime)
            return;
    }
#endif
    if (m_impl->streamServer->getClientCount() == 0) return;

#if defined(OPENEYE_STREAMING_H264) && OPENEYE_STREAMING_H264 && defined(_WIN32)
    if (!m_impl->h264Encoder) {
        tienet::media::EncoderConfig cfg;
        cfg.width = width;
        cfg.height = height;
        cfg.bitrateBps = Impl::kStreamBitrateBps;
        cfg.fps = Impl::kTargetStreamFps;
        cfg.codec = "h264";
        cfg.keyframeIntervalMs = (Impl::kGopFrames > 0u) ? (1000u * Impl::kGopFrames / Impl::kTargetStreamFps) : 1000u;
#ifdef TIENET_HAS_NVENC
        std::fprintf(stderr, "[VRStreaming] Using H264 encoder: nvenc (NV12 path), stream %u FPS\n", Impl::kTargetStreamFps);
        m_impl->h264Encoder = tienet::media::EncoderFactory::create("nvenc");
#else
        std::fprintf(stderr, "[VRStreaming] Using H264 encoder: mf (NV12 path)\n");
        m_impl->h264Encoder = tienet::media::EncoderFactory::create("mf");
#endif
        if (m_impl->h264Encoder && m_impl->h264Encoder->initialize(cfg)) {
            m_impl->encoderW = width;
            m_impl->encoderH = height;
        } else {
            std::fprintf(stderr, "[VRStreaming] H264 encoder init failed (NV12 path), encoder discarded\n");
            m_impl->h264Encoder.reset();
        }
    }
    if (m_impl->h264Encoder && (width != m_impl->encoderW || height != m_impl->encoderH)) {
        tienet::media::EncoderConfig cfg;
        cfg.width = width;
        cfg.height = height;
        cfg.bitrateBps = Impl::kStreamBitrateBps;
        cfg.fps = Impl::kTargetStreamFps;
        cfg.codec = "h264";
        cfg.keyframeIntervalMs = (Impl::kGopFrames > 0u) ? (1000u * Impl::kGopFrames / Impl::kTargetStreamFps) : 1000u;
        if (m_impl->h264Encoder->reconfigure(cfg)) {
            m_impl->encoderW = width;
            m_impl->encoderH = height;
        }
    }
    if (!m_impl->h264Encoder) {
        std::fprintf(stderr, "[VRStreaming] H264 encoder null\n");
        return;
    }
    const uint32_t rowPitch = (pitch != 0) ? pitch : width;
    const size_t nv12Size = static_cast<size_t>(rowPitch) * height + static_cast<size_t>(rowPitch) * (height / 2);
    std::vector<uint8_t> nv12Vec(nv12, nv12 + nv12Size);
    std::vector<uint8_t> h264 = m_impl->h264Encoder->encodeFromNV12(nv12Vec, rowPitch);
    if (h264.empty()) {
        m_impl->nv12EmptyCount++;
        if (m_impl->nv12EmptyCount <= 3 || (m_impl->nv12EmptyCount % 100) == 0)
            std::fprintf(stderr, "[VRStreaming] encodeFromNV12 returned empty (warmup frame %u)\n", m_impl->nv12EmptyCount);
        return;
    }
    if (!m_impl->nv12FirstSent) {
        m_impl->nv12FirstSent = true;
        std::fprintf(stderr, "[VRStreaming] first H.264 frame sent (%zu bytes, after %u warmup frames)\n", h264.size(), m_impl->nv12EmptyCount);
    }

    m_impl->streamServer->sendFramePayload(Impl::kFormatH264, h264.data(), h264.size());

    {
        const auto now = std::chrono::steady_clock::now();
        m_impl->nextSendTime += std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(Impl::kFrameIntervalSec));
        if (m_impl->nextSendTime < now)
            m_impl->nextSendTime = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(Impl::kFrameIntervalSec));
    }
#endif
}

float VRStreamingServer::GetStreamingFPS() const {
    if (!m_running) return 0.0f;
    return m_impl->streamServer->getStreamingFPS();
}

size_t VRStreamingServer::GetClientCount() const {
    return m_impl->streamServer->getClientCount();
}

std::string VRStreamingServer::GetLocalIPv4Address() {
    return tienet::streaming::StreamServer::getLocalIPv4Address();
}

} // namespace app
} // namespace eyelib
