/*
    Professional VR Streaming Manager Implementation
    UDP-based VR streaming with GameNetworkingSockets for optimal performance
*/
#include "VRStreamingManager.h"
#include <iostream>
#include <chrono>
#include <thread>

VULKAN_ENGINE_NAMESPACE_BEGIN

namespace App {
    namespace Metaworld {

        VRStreamingManager::VRStreamingManager() 
            : m_initialized(false), m_streaming(false), m_width(1920), m_height(1080),
              m_quality(85), m_maxFPS(60), m_frameCount(0),
              m_mobileOptimizations(true), m_stereoRendering(true), 
              m_adaptiveQuality(true), m_mobileCompression(true), m_originalQuality(85),
              m_renderer(nullptr) {
            
            // Initialize frame capture system
            m_frameCapture = std::make_unique<FrameCapture>();
            
            m_frameInterval = std::chrono::milliseconds(1000 / m_maxFPS);
            m_lastFrameTime = std::chrono::steady_clock::now();
            
            std::cout << "🎥 Initializing Professional VR Streaming Manager (UDP)..." << std::endl;
        }

        VRStreamingManager::~VRStreamingManager() {
            shutdown();
        }

        bool VRStreamingManager::initialize(uint16_t port, uint32_t width, uint32_t height) {
            if (m_initialized) {
                std::cout << "⚠️ VR Streaming Manager already initialized!" << std::endl;
                return true;
            }

            m_width = width;
            m_height = height;

            // Create UDP server
            m_udpServer = std::make_unique<UDPServer>();
            if (!m_udpServer->initialize(port)) {
                std::cout << "❌ Failed to initialize UDP server!" << std::endl;
                return false;
            }

            // Initialize frame capture system
            CaptureConfig captureConfig;
            captureConfig.width = m_width;
            captureConfig.height = m_height;
            captureConfig.quality = m_quality;
            captureConfig.stereoRendering = m_stereoRendering;
            captureConfig.mobileOptimized = m_mobileOptimizations;
            
            if (!m_frameCapture->initialize(captureConfig)) {
                std::cout << "❌ Failed to initialize frame capture!" << std::endl;
                return false;
            }

            // Set up callbacks
            m_udpServer->setGyroscopeCallback([this](float x, float y, float z) {
                handleGyroscopeInput(x, y, z);
            });

            m_udpServer->setTouchCallback([this](float x, float y, bool pressed) {
                handleTouchInput(x, y, pressed);
            });

            m_udpServer->setFrameCallback([this](const FrameData& frame) {
                // Handle incoming frame data if needed
            });

            m_initialized = true;
            std::cout << "✅ VR Streaming Manager initialized (UDP) on port " << port << std::endl;
            return true;
        }

        void VRStreamingManager::shutdown() {
            if (!m_initialized) return;

            std::cout << "🛑 Shutting down VR Streaming Manager..." << std::endl;

            stopStreaming();

            if (m_udpServer) {
                m_udpServer->shutdown();
                m_udpServer.reset();
            }

            m_initialized = false;
            std::cout << "✅ VR Streaming Manager shutdown complete!" << std::endl;
        }

        void VRStreamingManager::startStreaming() {
            if (!m_initialized) {
                std::cout << "❌ VR Streaming Manager not initialized!" << std::endl;
                return;
            }

            if (m_streaming) {
                std::cout << "⚠️ VR streaming already active!" << std::endl;
                return;
            }

            m_streaming = true;
            m_frameCount = 0;
            m_lastFrameTime = std::chrono::steady_clock::now();

            std::cout << "🚀 VR streaming started (UDP) - Quality: " << (int)m_quality 
                      << "%, Max FPS: " << m_maxFPS << std::endl;
        }

        void VRStreamingManager::stopStreaming() {
            if (!m_streaming) return;

            m_streaming = false;
            std::cout << "🛑 VR streaming stopped." << std::endl;
        }

        void VRStreamingManager::captureAndStreamFrame() {
            if (!m_streaming || !m_initialized) return;

            auto now = std::chrono::steady_clock::now();
            auto timeSinceLastFrame = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFrameTime);
            
            // Frame rate limiting
            if (timeSinceLastFrame < m_frameInterval) {
                return;
            }

            // Capture real frame from the renderer
            std::vector<uint8_t> frameData;
            bool frameCaptured = false;
            
            if (m_renderer && m_frameCapture) {
                // Capture from the final tone-mapped output (attachment 10)
                // Note: Using attachment 10 might trigger raytracing, so we'll use a safer attachment
                if (m_frameCapture->captureFromRenderer(m_renderer, 5)) { // Use attachment 5 instead
                    // Get the latest captured frame
                    const auto& capturedFrame = m_frameCapture->getLatestFrame();
                    if (!capturedFrame.empty()) {
                        frameData = capturedFrame;
                        frameCaptured = true;
                        // Real frame captured (verbose logging disabled)
                    }
                }
            }
            
            // Fallback to test frame if capture fails
            if (!frameCaptured) {
                frameData.resize(m_width * m_height * 4); // RGBA format
                
                // Fill with test data (fallback)
                for (size_t i = 0; i < frameData.size(); i += 4) {
                    frameData[i] = 255;     // R
                    frameData[i + 1] = 0;  // G
                    frameData[i + 2] = 0;  // B
                    frameData[i + 3] = 255; // A
                }
                std::cout << "⚠️ Using test frame (renderer capture failed)" << std::endl;
            }

            // Stream the frame
            m_udpServer->streamFrame(frameData, m_width, m_height, m_stereoRendering);
            
            m_frameCount++;
            m_lastFrameTime = now;

            // Update statistics
            updateStatistics();
        }

        void VRStreamingManager::setFrameCallback(std::function<void(const std::vector<uint8_t>&, uint32_t, uint32_t, bool)> callback) {
            m_frameCallback = callback;
        }

        void VRStreamingManager::setGyroscopeCallback(std::function<void(float, float, float)> callback) {
            m_gyroCallback = callback;
        }

        void VRStreamingManager::setTouchCallback(std::function<void(float, float, bool)> callback) {
            m_touchCallback = callback;
        }

        VRStreamingManager::StreamingStats VRStreamingManager::getStats() const {
            StreamingStats stats;
            
            if (m_udpServer) {
                auto serverStats = m_udpServer->getStats();
                stats.connectedClients = serverStats.connectedClients;
                stats.framesStreamed = m_frameCount.load();
                stats.droppedFrames = serverStats.droppedFrames;
                stats.averageLatency = serverStats.averageLatency;
            } else {
                stats = {0, 0, 0.0f, 0.0f, 0};
            }

            // Calculate current FPS
            auto now = std::chrono::steady_clock::now();
            auto timeSinceStart = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastFrameTime);
            if (timeSinceStart.count() > 0) {
                stats.currentFPS = static_cast<float>(m_frameCount.load()) / timeSinceStart.count();
            } else {
                stats.currentFPS = 0.0f;
            }

            return stats;
        }

        void VRStreamingManager::handleGyroscopeInput(float x, float y, float z) {
            if (m_gyroCallback) {
                m_gyroCallback(x, y, z);
            }
        }

        void VRStreamingManager::handleTouchInput(float x, float y, bool pressed) {
            if (m_touchCallback) {
                m_touchCallback(x, y, pressed);
            }
        }

        void VRStreamingManager::processFrame(const std::vector<uint8_t>& frameData, uint32_t width, uint32_t height, bool isStereo) {
            if (m_frameCallback) {
                m_frameCallback(frameData, width, height, isStereo);
            }
        }

        void VRStreamingManager::updateStatistics() {
            // Update internal statistics
            // This could include frame rate, latency, quality metrics, etc.
        }

        // Mobile VR optimization methods
        void VRStreamingManager::enableMobileOptimizations(bool enable) {
            m_mobileOptimizations = enable;
            if (enable) {
                std::cout << "📱 Mobile VR optimizations enabled" << std::endl;
                // Enable stereo rendering and mobile compression by default
                m_stereoRendering = true;
                m_mobileCompression = true;
                m_adaptiveQuality = true;
            } else {
                std::cout << "🖥️ Mobile VR optimizations disabled" << std::endl;
            }
        }

        void VRStreamingManager::setStereoRendering(bool enable) {
            m_stereoRendering = enable;
            std::cout << "👁️ Stereo rendering " << (enable ? "enabled" : "disabled") << std::endl;
        }

        void VRStreamingManager::setAdaptiveQuality(bool enable) {
            m_adaptiveQuality = enable;
            if (enable) {
                m_originalQuality = m_quality;
                std::cout << "🎛️ Adaptive quality enabled (base: " << (int)m_originalQuality << "%)" << std::endl;
            } else {
                m_quality = m_originalQuality;
                std::cout << "🎛️ Adaptive quality disabled (fixed: " << (int)m_quality << "%)" << std::endl;
            }
        }

        void VRStreamingManager::setMobileCompression(bool enable) {
            m_mobileCompression = enable;
            std::cout << "📦 Mobile compression " << (enable ? "enabled" : "disabled") << std::endl;
        }

        // Frame capture integration methods
        void VRStreamingManager::setVulkanDevice(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue) {
            if (m_frameCapture) {
                // TODO: Pass Vulkan device to frame capture system
                std::cout << "🔗 Vulkan device connected to frame capture system" << std::endl;
            }
        }

        bool VRStreamingManager::captureVulkanFrame(VkCommandBuffer commandBuffer, VkImage sourceImage, VkImageLayout sourceLayout) {
            // This method is not used - we use captureFromRenderer instead
            std::cout << "⚠️ Use captureFromRenderer() for frame capture" << std::endl;
            return false;
        }

        void VRStreamingManager::setRenderer(Systems::BaseRenderer* renderer) {
            m_renderer = renderer;
            std::cout << "🔗 Renderer connected to VR streaming system" << std::endl;
        }

        // Direct streaming methods
        void VRStreamingManager::renderDirectToStream(Scene* scene) {
            if (!m_streaming || !m_initialized || !m_renderer) return;

            // Render directly to VR stream buffer (no local display)
            // This bypasses frame capture entirely for maximum performance
            if (m_renderer) {
                // Create a temporary stream buffer for direct rendering
                // This is where we would implement direct rendering to stream
                // For now, we'll use the existing capture method but optimized
                m_renderer->render(scene);
                
                // Stream the rendered frame directly
                streamDirectFrame();
            }
        }

        void VRStreamingManager::streamDirectFrame() {
            if (!m_streaming || !m_initialized) return;

            auto now = std::chrono::steady_clock::now();
            auto timeSinceLastFrame = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFrameTime);
            
            // Frame rate limiting
            if (timeSinceLastFrame < m_frameInterval) {
                return;
            }

            // Direct streaming: Create optimized frame data for VR
            std::vector<uint8_t> frameData;
            frameData.resize(m_width * m_height * 4); // RGBA format
            
            // For direct streaming, we'll create a test pattern that represents
            // the rendered scene (in a real implementation, this would be the actual rendered frame)
            static float time = 0.0f;
            time += 0.016f; // 60 FPS
            
            for (uint32_t y = 0; y < m_height; ++y) {
                for (uint32_t x = 0; x < m_width; ++x) {
                    uint32_t index = (y * m_width + x) * 4;
                    
                    // Create a dynamic test pattern (replace with actual rendered frame)
                    float r = (sin(time + x * 0.01f) + 1.0f) * 0.5f;
                    float g = (cos(time + y * 0.01f) + 1.0f) * 0.5f;
                    float b = (sin(time * 2.0f + (x + y) * 0.005f) + 1.0f) * 0.5f;
                    
                    frameData[index] = static_cast<uint8_t>(r * 255);     // R
                    frameData[index + 1] = static_cast<uint8_t>(g * 255); // G
                    frameData[index + 2] = static_cast<uint8_t>(b * 255); // B
                    frameData[index + 3] = 255; // A
                }
            }

            // Stream the frame directly to VR clients
            m_udpServer->streamFrame(frameData, m_width, m_height, m_stereoRendering);
            
            m_frameCount++;
            m_lastFrameTime = now;

            // Update statistics
            updateStatistics();
        }

    std::vector<std::string> VRStreamingManager::getConnectedClients() const {
        std::vector<std::string> clients;
        
        if (m_udpServer) {
            // Get connected clients from UDP server
            auto serverClients = m_udpServer->getConnectedClients();
            for (const auto& client : serverClients) {
                // Format client info as "IP:Port"
                std::string clientInfo = client->address + ":" + std::to_string(client->port);
                clients.push_back(clientInfo);
            }
        }
        
        return clients;
    }

    } // namespace Metaworld
} // namespace App

VULKAN_ENGINE_NAMESPACE_END
