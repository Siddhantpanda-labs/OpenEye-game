/*
    Professional Frame Capture System for VR Streaming
    Captures Vulkan framebuffer data for real-time VR streaming
*/
#ifndef FRAME_CAPTURE_H
#define FRAME_CAPTURE_H

#include <memory>
#include <vector>
#include <functional>
#include <engine/common.h>
#include <engine/systems/renderers/renderer.h>

VULKAN_ENGINE_NAMESPACE_BEGIN

namespace App {
    namespace Metaworld {

        // Frame capture configuration
        struct CaptureConfig {
            uint32_t width = 1920;
            uint32_t height = 1080;
            uint32_t quality = 85;
            bool stereoRendering = true;
            bool mobileOptimized = true;
        };

        // Captured frame data
        struct CapturedFrame {
            std::vector<uint8_t> data;
            uint32_t width;
            uint32_t height;
            uint32_t channels;
            uint64_t timestamp;
            uint32_t frameId;
            bool isStereo;
            
            CapturedFrame() : width(0), height(0), channels(4), timestamp(0), frameId(0), isStereo(false) {}
        };

        // Professional Frame Capture System
        class FrameCapture {
        public:
            FrameCapture();
            ~FrameCapture();

            // Initialization
            bool initialize(const CaptureConfig& config);
            void shutdown();

            // Frame capture - Main method used by VR streaming
            bool captureFromRenderer(Systems::BaseRenderer* renderer, uint32_t attachmentId = 10);
            const std::vector<uint8_t>& getLatestFrame() const;
            
            // Configuration
            CaptureConfig getConfig() const { return m_config; }

            // Statistics
            struct CaptureStats {
                uint32_t framesCaptured;
                uint32_t framesProcessed;
                float averageCaptureTime;
                float averageProcessingTime;
                uint32_t droppedFrames;
            };
            CaptureStats getStats() const { return m_stats; }

        private:
            // Configuration
            CaptureConfig m_config;
            bool m_initialized;
            
            // Frame processing
            std::vector<CapturedFrame> m_frameQueue;
            std::mutex m_frameQueueMutex;
            
            // Statistics
            CaptureStats m_stats;
            std::chrono::steady_clock::time_point m_lastStatsUpdate;
        };

    } // namespace Metaworld
} // namespace App

VULKAN_ENGINE_NAMESPACE_END

#endif // FRAME_CAPTURE_H
