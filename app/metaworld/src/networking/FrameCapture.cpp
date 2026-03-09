/*
    Professional Frame Capture System Implementation
    Captures Vulkan framebuffer data for real-time VR streaming
*/
#include "FrameCapture.h"
#include <iostream>
#include <chrono>
#include <cstring>

VULKAN_ENGINE_NAMESPACE_BEGIN

namespace App {
    namespace Metaworld {

        FrameCapture::FrameCapture() 
            : m_initialized(false), m_lastStatsUpdate(std::chrono::steady_clock::now()) {
            
            // Initialize statistics
            m_stats = {0, 0, 0.0f, 0.0f, 0};
            
            std::cout << "📸 Initializing Professional Frame Capture System..." << std::endl;
        }

        FrameCapture::~FrameCapture() {
            shutdown();
        }

        bool FrameCapture::initialize(const CaptureConfig& config) {
            if (m_initialized) {
                std::cout << "⚠️ Frame capture already initialized!" << std::endl;
                return true;
            }

            m_config = config;
            
            std::cout << "📸 Frame capture initialized with config:" << std::endl;
            std::cout << "   Resolution: " << m_config.width << "x" << m_config.height << std::endl;
            std::cout << "   Quality: " << m_config.quality << "%" << std::endl;
            std::cout << "   Stereo: " << (m_config.stereoRendering ? "Yes" : "No") << std::endl;
            std::cout << "   Mobile Optimized: " << (m_config.mobileOptimized ? "Yes" : "No") << std::endl;

            m_initialized = true;
            return true;
        }

        void FrameCapture::shutdown() {
            if (!m_initialized) return;

            std::cout << "🛑 Shutting down Frame Capture System..." << std::endl;

            // Clear frame queue
            {
                std::lock_guard<std::mutex> lock(m_frameQueueMutex);
                m_frameQueue.clear();
            }

            // Cleanup completed

            m_initialized = false;
            std::cout << "✅ Frame capture shutdown complete!" << std::endl;
        }





        bool FrameCapture::captureFromRenderer(Systems::BaseRenderer* renderer, uint32_t attachmentId) {
            if (!m_initialized) {
                std::cout << "❌ Frame capture not initialized!" << std::endl;
                return false;
            }
            
            if (!renderer) {
                std::cout << "❌ No renderer provided for frame capture!" << std::endl;
                return false;
            }
            
            try {
                auto startTime = std::chrono::high_resolution_clock::now();
                
                // Use the renderer's built-in texture capture
                Core::ITexture* capturedTexture = renderer->capture_texture(attachmentId);
                if (!capturedTexture) {
                    std::cout << "❌ Failed to capture texture from renderer!" << std::endl;
                    return false;
                }
                
                // Get the captured texture data
                void* textureDataPtr = nullptr;
                capturedTexture->get_image_cache(textureDataPtr);
                const auto& extent = capturedTexture->get_size();
                
                if (!textureDataPtr) {
                    std::cout << "❌ No texture data available!" << std::endl;
                    delete capturedTexture;
                    return false;
                }
                
                // Create captured frame
                CapturedFrame frame;
                frame.width = extent.width;
                frame.height = extent.height;
                frame.channels = 4; // RGBA
                frame.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                frame.frameId = m_stats.framesCaptured;
                frame.isStereo = m_config.stereoRendering;
                
                // Copy texture data
                size_t dataSize = extent.width * extent.height * 4; // RGBA
                frame.data.resize(dataSize);
                memcpy(frame.data.data(), textureDataPtr, dataSize);
                
                // Add to frame queue
                {
                    std::lock_guard<std::mutex> lock(m_frameQueueMutex);
                    m_frameQueue.push_back(std::move(frame));
                    
                    // Limit queue size to prevent memory issues
                    if (m_frameQueue.size() > 10) { // MAX_FRAME_QUEUE_SIZE
                        m_frameQueue.erase(m_frameQueue.begin());
                    }
                }
                
                // Update statistics
                m_stats.framesCaptured++;
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                // m_stats.totalCaptureTime += duration.count(); // This field doesn't exist
                
                // Clean up the captured texture
                delete capturedTexture;
                
                // Frame captured successfully (verbose logging disabled)
                
                return true;
                
            } catch (const std::exception& e) {
                std::cout << "❌ Exception during frame capture: " << e.what() << std::endl;
                return false;
            }
        }

        const std::vector<uint8_t>& FrameCapture::getLatestFrame() const {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_frameQueueMutex));
            if (!m_frameQueue.empty()) {
                return m_frameQueue.back().data;
            }
            static const std::vector<uint8_t> emptyFrame;
            return emptyFrame;
        }

    } // namespace Metaworld
} // namespace App

VULKAN_ENGINE_NAMESPACE_END
