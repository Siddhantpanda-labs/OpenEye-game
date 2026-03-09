/*
    Professional VR Streaming Manager
    UDP-based VR streaming with GameNetworkingSockets for optimal performance
*/
#ifndef VR_STREAMING_MANAGER_H
#define VR_STREAMING_MANAGER_H

#include <memory>
#include <functional>
#include <iostream>
#include <engine/common.h>
#include "UDPServer.h"
#include "FrameCapture.h"

VULKAN_ENGINE_NAMESPACE_BEGIN

namespace App {
    namespace Metaworld {

        // VR Streaming Manager for UDP-based VR streaming
        class VRStreamingManager {
        public:
            VRStreamingManager();
            ~VRStreamingManager();

            // Initialization
            bool initialize(uint16_t port = 27015, uint32_t width = 1280, uint32_t height = 720);
            void shutdown();

            // Streaming control
            void startStreaming();
            void stopStreaming();
            bool isStreaming() const { return m_streaming; }

            // Frame capture and streaming
            void captureAndStreamFrame();
            void setFrameCallback(std::function<void(const std::vector<uint8_t>&, uint32_t, uint32_t, bool)> callback);
            
            // Direct streaming methods
            void renderDirectToStream(Scene* scene);
            void streamDirectFrame();
            
        // Frame capture integration
        void setVulkanDevice(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue);
        bool captureVulkanFrame(VkCommandBuffer commandBuffer, VkImage sourceImage, VkImageLayout sourceLayout);
        void setRenderer(Systems::BaseRenderer* renderer);

            // Input handling
            void setGyroscopeCallback(std::function<void(float, float, float)> callback);
            void setTouchCallback(std::function<void(float, float, bool)> callback);

            // Configuration
            void setStreamingQuality(uint8_t quality) { m_quality = quality; }
            
            // Client management
            std::vector<std::string> getConnectedClients() const;
            void setMaxFPS(uint32_t fps) { m_maxFPS = fps; }
            void setResolution(uint32_t width, uint32_t height) { m_width = width; m_height = height; }
            
            // Mobile VR optimizations
            void enableMobileOptimizations(bool enable = true);
            void setStereoRendering(bool enable = true);
            void setAdaptiveQuality(bool enable = true);
            void setMobileCompression(bool enable = true);

            // Statistics
            struct StreamingStats {
                uint32_t connectedClients;
                uint32_t framesStreamed;
                float currentFPS;
                float averageLatency;
                uint32_t droppedFrames;
            };
            StreamingStats getStats() const;

        private:
        // Core components
        std::unique_ptr<UDPServer> m_udpServer;
        std::unique_ptr<FrameCapture> m_frameCapture;
        Systems::BaseRenderer* m_renderer;
            
            // Streaming state
            std::atomic<bool> m_initialized;
            std::atomic<bool> m_streaming;
            uint32_t m_width, m_height;
            uint8_t m_quality;
            uint32_t m_maxFPS;
            
            // Frame management
            std::atomic<uint32_t> m_frameCount;
            std::chrono::steady_clock::time_point m_lastFrameTime;
            std::chrono::milliseconds m_frameInterval;
            
            // Mobile VR optimizations
            bool m_mobileOptimizations;
            bool m_stereoRendering;
            bool m_adaptiveQuality;
            bool m_mobileCompression;
            uint32_t m_originalQuality;
            
            // Callbacks
            std::function<void(const std::vector<uint8_t>&, uint32_t, uint32_t, bool)> m_frameCallback;
            std::function<void(float, float, float)> m_gyroCallback;
            std::function<void(float, float, bool)> m_touchCallback;
            
            // Internal methods
            void handleGyroscopeInput(float x, float y, float z);
            void handleTouchInput(float x, float y, bool pressed);
            void processFrame(const std::vector<uint8_t>& frameData, uint32_t width, uint32_t height, bool isStereo);
            void updateStatistics();
        };

    } // namespace Metaworld
} // namespace App

VULKAN_ENGINE_NAMESPACE_END

#endif // VR_STREAMING_MANAGER_H
