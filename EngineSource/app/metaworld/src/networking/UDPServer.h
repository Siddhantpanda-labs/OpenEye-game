/*
    Professional UDP Server for VR Streaming
    Using Valve's GameNetworkingSockets for optimal real-time performance
*/
#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <engine/common.h>
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>
#include <queue>

VULKAN_ENGINE_NAMESPACE_BEGIN

namespace App {
    namespace Metaworld {

        // VR Client connection info
        struct VRClient {
            HSteamNetConnection connection;
            std::string clientId;
            std::string address;
            uint16_t port;
            std::chrono::steady_clock::time_point lastPing;
            bool isActive;
            float latency;
            
            VRClient() : connection(k_HSteamNetConnection_Invalid), port(0), isActive(false), latency(0.0f) {}
        };


        // Gyroscope input data
        struct GyroInput {
            float x, y, z;
            std::chrono::steady_clock::time_point timestamp;
        };

        // Frame data structure
        struct FrameData {
            std::vector<uint8_t> data;
            uint32_t width;
            uint32_t height;
            uint64_t timestamp;
            uint32_t frameId;
            bool isStereo;
            
            FrameData() : width(0), height(0), timestamp(0), frameId(0), isStereo(false) {}
        };

        // UDP Server for VR Streaming
        class UDPServer {
        public:
            UDPServer();
            ~UDPServer();

            // Server lifecycle
            bool initialize(uint16_t port = 27015);
            void shutdown();
            bool isRunning() const { return m_running; }

            // Client management
            void broadcastMessage(const std::string& message);
            std::vector<VRClient*> getConnectedClients() const;
            void sendToClient(const std::string& clientId, const std::string& message);
            void sendFrameToClient(const std::string& clientId, const FrameData& frame);
            void broadcastFrame(const FrameData& frame);

            // VR streaming
            void streamFrame(const std::vector<uint8_t>& frameData, uint32_t width, uint32_t height, bool isStereo = false);
            void setFrameCallback(std::function<void(const FrameData&)> callback);

            // Input handling
            void setGyroscopeCallback(std::function<void(float, float, float)> callback);
            void setTouchCallback(std::function<void(float, float, bool)> callback);

            // Server statistics
            struct ServerStats {
                uint32_t connectedClients;
                uint32_t totalFramesSent;
                uint32_t totalBytesSent;
                float averageLatency;
                uint32_t droppedFrames;
            };
            ServerStats getStats() const;


        private:
            // Steam Networking (streaming only)
            ISteamNetworkingSockets* m_networkingSockets;
            ISteamNetworkingUtils* m_networkingUtils;
            HSteamListenSocket m_listenSocket;
            HSteamNetConnection m_clientConnection;
            
            // Server state
            std::atomic<bool> m_running;
            std::atomic<bool> m_initialized;
            uint16_t m_port;
            
            // Client management
            std::vector<std::unique_ptr<VRClient>> m_clients;
            std::mutex m_clientsMutex;
            std::string m_nextClientId;
            
            // Frame streaming
            std::queue<FrameData> m_frameQueue;
            std::mutex m_frameQueueMutex;
            std::atomic<uint32_t> m_frameCounter;
            
            // Callbacks
            std::function<void(const FrameData&)> m_frameCallback;
            std::function<void(float, float, float)> m_gyroCallback;
            std::function<void(float, float, bool)> m_touchCallback;
            
            // Threading
            std::thread m_serverThread;
            std::thread m_frameProcessingThread;
            std::atomic<bool> m_shouldStop;
            
            // Statistics
            mutable std::mutex m_statsMutex;
            ServerStats m_stats;
            
            // Discovery methods (GameNetworkingSockets-based)
            void handleDiscoveryRequest(VRClient* client);
            void handleDiscoveryConnection(HSteamNetConnection conn);
            
            // Client management
            VRClient* findClient(const std::string& clientId);
            VRClient* findClient(HSteamNetConnection connection);
            std::string generateClientId();
            void cleanupInactiveClients();
            void updateClientLatency(VRClient* client);
            
            // Internal methods
            void runServer();
            void processFrames();
            void handleClientMessage(VRClient* client, const std::string& message);
            void handleClientConnect(VRClient* client);
            void handleGyroscopeData(const std::string& data);
            void handleTouchData(const std::string& data);
            void handlePing(const std::string& data);
            
            // Frame processing
            void compressFrame(const FrameData& frame, std::vector<uint8_t>& compressed);
            void createFrameMessage(const FrameData& frame, std::string& message);
            void sendFrameToClientInternal(VRClient* client, const FrameData& frame);
            
            // Statistics
            void updateStats();
        };

    } // namespace Metaworld
} // namespace App

VULKAN_ENGINE_NAMESPACE_END

#endif // UDP_SERVER_H
